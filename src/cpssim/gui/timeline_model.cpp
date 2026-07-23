/***
 * File: src/cpssim/gui/timeline_model.cpp
 * Purpose: Strictly derive G05 lifecycle intervals and canonical markers.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/timeline_model.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace cpssim {
namespace {

enum class DerivedJobState {
    Ready,
    Running,
    Completed,
};

struct JobBuildState {
    ResourceId resource_id;
    DerivedJobState state;
    bool has_started;
    Tick interval_begin;
    EventSequence interval_begin_sequence;
    bool running_interval_resumed;
};

int phase_precedence(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return 0;
    case EventPhase::MessageDelivery:
        return 1;
    case EventPhase::DeadlineCheck:
        return 2;
    case EventPhase::JobRelease:
        return 3;
    case EventPhase::PolicyUpdate:
        return 4;
    case EventPhase::Scheduling:
        return 5;
    case EventPhase::CausedAction:
        return 6;
    case EventPhase::CausedActionLate:
        return 7;
    }
    return -1;
}

const char* phase_name(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return "execution_completion";
    case EventPhase::MessageDelivery:
        return "message_delivery";
    case EventPhase::DeadlineCheck:
        return "deadline_check";
    case EventPhase::JobRelease:
        return "job_release";
    case EventPhase::PolicyUpdate:
        return "policy_update";
    case EventPhase::Scheduling:
        return "scheduling";
    case EventPhase::CausedAction:
        return "caused_action";
    case EventPhase::CausedActionLate:
        return "caused_action_late";
    }
    return "unknown";
}

std::optional<EventPhase> required_phase(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return EventPhase::JobRelease;
    case EventType::JobStart:
    case EventType::JobPreempt:
    case EventType::JobResume:
        return EventPhase::Scheduling;
    case EventType::JobFinish:
        return EventPhase::ExecutionCompletion;
    case EventType::DeadlineMiss:
        return EventPhase::DeadlineCheck;
    case EventType::MessageSend:
        return EventPhase::CausedAction;
    case EventType::MessageDelivery:
        return EventPhase::MessageDelivery;
    }
    return std::nullopt;
}

std::string job_name(JobIdentity job) {
    return "T" + std::to_string(job.task_id().value()) + ":J" +
           std::to_string(job.job_id().value());
}

GuiTimelineBuildResult fail(GuiTimelineDiagnosticCode code, std::size_t event_index,
                            const Event& event, std::string reason) {
    const auto message = "timeline event[" + std::to_string(event_index) + "] sequence " +
                         std::to_string(event.sequence().value()) + " at tick " +
                         std::to_string(event.tick()) + ": " + std::move(reason);
    return {.timeline = std::nullopt,
            .diagnostics = {{.code = code,
                             .event_index = event_index,
                             .event_sequence = event.sequence(),
                             .tick = event.tick(),
                             .message = message}}};
}

GuiTimelineBuildResult fail_current_tick(std::size_t event_count, Tick current_tick,
                                         std::string reason) {
    return {.timeline = std::nullopt,
            .diagnostics = {{.code = GuiTimelineDiagnosticCode::InvalidCurrentTick,
                             .event_index = event_count,
                             .event_sequence = std::nullopt,
                             .tick = current_tick,
                             .message = "timeline current_tick " + std::to_string(current_tick) +
                                        ": " + std::move(reason)}}};
}

std::optional<JobIdentity> event_job(const Event& event) {
    const auto& entities = event.entities();
    if (!entities.task_id.has_value() || !entities.job_id.has_value()) {
        return std::nullopt;
    }
    return JobIdentity{entities.task_id.value(), entities.job_id.value()};
}

GuiTimelineRow* find_row(std::vector<GuiTimelineRow>& rows, ResourceId resource_id) {
    const auto found = std::lower_bound(
        rows.begin(), rows.end(), resource_id,
        [](const GuiTimelineRow& row, ResourceId id) { return row.resource_id < id; });
    return found != rows.end() && found->resource_id == resource_id ? &*found : nullptr;
}

GuiTimelineBuildResult missing_entity(std::size_t index, const Event& event,
                                      std::string entity_name) {
    return fail(GuiTimelineDiagnosticCode::MissingEntity, index, event,
                "event type requires " + std::move(entity_name));
}

GuiTimelineBuildResult validate_known_entities(std::size_t index, const Event& event,
                                               const ExperimentPresentationSnapshot& experiment,
                                               bool require_job, bool require_resource,
                                               bool require_message) {
    const auto& entities = event.entities();
    if (!entities.task_id.has_value()) {
        return missing_entity(index, event, "task_id");
    }
    if (find_task(experiment, entities.task_id.value()) == nullptr) {
        return fail(GuiTimelineDiagnosticCode::UnknownEntity, index, event,
                    "task T" + std::to_string(entities.task_id.value().value()) +
                        " is unavailable");
    }
    if (require_job && !entities.job_id.has_value()) {
        return missing_entity(index, event, "job_id");
    }
    if (require_resource && !entities.resource_id.has_value()) {
        return missing_entity(index, event, "resource_id");
    }
    if (entities.resource_id.has_value() &&
        find_resource(experiment, entities.resource_id.value()) == nullptr) {
        return fail(GuiTimelineDiagnosticCode::UnknownEntity, index, event,
                    "resource R" + std::to_string(entities.resource_id.value().value()) +
                        " is unavailable");
    }
    if (require_message && !entities.message_id.has_value()) {
        return missing_entity(index, event, "message_id");
    }
    return {};
}

GuiTimelineBuildResult append_interval(std::vector<GuiTimelineRow>& rows,
                                       GuiTimelineIntervalKind kind, JobIdentity job,
                                       const JobBuildState& state, Tick end_tick,
                                       EventSequence end_sequence) {
    if (end_tick < state.interval_begin) {
        return {
            .timeline = std::nullopt,
            .diagnostics = {{.code = GuiTimelineDiagnosticCode::InvalidInterval,
                             .event_index = 0,
                             .event_sequence = end_sequence,
                             .tick = end_tick,
                             .message = "timeline interval for " + job_name(job) + ": end tick " +
                                        std::to_string(end_tick) + " precedes begin tick " +
                                        std::to_string(state.interval_begin)}}};
    }
    auto* row = find_row(rows, state.resource_id);
    if (row == nullptr) {
        throw std::logic_error{"validated timeline resource row is unavailable"};
    }
    row->intervals.push_back(
        {.kind = kind,
         .job = job,
         .resource_id = state.resource_id,
         .begin_tick = state.interval_begin,
         .end_tick = end_tick,
         .begin_sequence = state.interval_begin_sequence,
         .end_sequence = end_sequence,
         .resumed = kind == GuiTimelineIntervalKind::Running && state.running_interval_resumed});
    return {};
}

void append_open_interval(std::vector<GuiTimelineRow>& rows, JobIdentity job,
                          const JobBuildState& state) {
    auto* row = find_row(rows, state.resource_id);
    if (row == nullptr) {
        throw std::logic_error{"validated timeline resource row is unavailable"};
    }
    row->intervals.push_back(
        {.kind = state.state == DerivedJobState::Ready ? GuiTimelineIntervalKind::Ready
                                                       : GuiTimelineIntervalKind::Running,
         .job = job,
         .resource_id = state.resource_id,
         .begin_tick = state.interval_begin,
         .end_tick = std::nullopt,
         .begin_sequence = state.interval_begin_sequence,
         .end_sequence = std::nullopt,
         .resumed = state.state == DerivedJobState::Running && state.running_interval_resumed});
}

bool interval_less(const GuiTimelineInterval& left, const GuiTimelineInterval& right) {
    if (left.begin_tick != right.begin_tick) {
        return left.begin_tick < right.begin_tick;
    }
    if (left.kind != right.kind) {
        return left.kind < right.kind;
    }
    if (left.job.task_id() != right.job.task_id()) {
        return left.job.task_id() < right.job.task_id();
    }
    if (left.job.job_id() != right.job.job_id()) {
        return left.job.job_id() < right.job.job_id();
    }
    return left.begin_sequence < right.begin_sequence;
}

bool same_event(const Event& left, const Event& right) {
    const auto& left_entities = left.entities();
    const auto& right_entities = right.entities();
    return left.tick() == right.tick() && left.phase() == right.phase() &&
           left.sequence() == right.sequence() && left.type() == right.type() &&
           left_entities.task_id == right_entities.task_id &&
           left_entities.job_id == right_entities.job_id &&
           left_entities.resource_id == right_entities.resource_id &&
           left_entities.message_id == right_entities.message_id &&
           left_entities.vehicle_id == right_entities.vehicle_id &&
           left.cause_sequence() == right.cause_sequence();
}

/*** Retains the validated prefix and mutable lifecycle derivation state. ***/
class TimelineDerivation {
  public:
    explicit TimelineDerivation(const ExperimentPresentationSnapshot& experiment)
        : experiment_{experiment},
          timeline_{.current_tick = 0, .rows = {}, .markers = {}, .marker_index = {}} {
        timeline_.rows.reserve(experiment.resources.size());
        for (const auto& resource : experiment.resources) {
            timeline_.rows.push_back(
                {.resource_id = resource.id, .label = resource.name, .intervals = {}});
        }
        std::sort(timeline_.rows.begin(), timeline_.rows.end(),
                  [](const GuiTimelineRow& left, const GuiTimelineRow& right) {
                      return left.resource_id < right.resource_id;
                  });
        for (const auto& row : timeline_.rows) {
            running_by_resource_.emplace(row.resource_id, std::nullopt);
        }
    }

    GuiTimelineBuildResult append(const Event& event, std::size_t index, Tick current_tick) {
        if (event.tick() > current_tick) {
            return fail(GuiTimelineDiagnosticCode::InvalidCurrentTick, index, event,
                        "event tick is later than current_tick " + std::to_string(current_tick));
        }
        if (previous_tick_.has_value()) {
            if (!previous_phase_.has_value()) {
                throw std::logic_error{"timeline previous tick has no phase"};
            }
            const auto previous_tick = previous_tick_.value();
            if (event.tick() < previous_tick ||
                (event.tick() == previous_tick &&
                 phase_precedence(event.phase()) < phase_precedence(previous_phase_.value()))) {
                return fail(GuiTimelineDiagnosticCode::InvalidTraceOrder, index, event,
                            "phase " + std::string{phase_name(event.phase())} +
                                " is out of canonical tick/phase order");
            }
        }
        if (!observed_sequences_.insert(event.sequence()).second) {
            return fail(GuiTimelineDiagnosticCode::DuplicateEventSequence, index, event,
                        "event sequence is duplicated");
        }
        const auto cause_sequence = event.cause_sequence();
        if (cause_sequence.has_value() && !observed_sequences_.contains(cause_sequence.value())) {
            return fail(GuiTimelineDiagnosticCode::UnobservedCause, index, event,
                        "cause sequence " + std::to_string(cause_sequence.value().value()) +
                            " has not appeared earlier in the canonical trace");
        }
        const auto expected_phase = required_phase(event.type());
        if (!expected_phase.has_value()) {
            return fail(GuiTimelineDiagnosticCode::WrongEventPhase, index, event,
                        "event type requires phase unknown, found " +
                            std::string{phase_name(event.phase())});
        }
        if (event.phase() != expected_phase.value()) {
            return fail(GuiTimelineDiagnosticCode::WrongEventPhase, index, event,
                        "event type requires phase " +
                            std::string{phase_name(expected_phase.value())} + ", found " +
                            phase_name(event.phase()));
        }

        const auto requires_job = event.type() != EventType::MessageDelivery;
        const auto requires_resource =
            event.type() != EventType::MessageDelivery && event.type() != EventType::MessageSend;
        auto shape = validate_known_entities(
            index, event, experiment_, requires_job, requires_resource,
            event.type() == EventType::MessageSend || event.type() == EventType::MessageDelivery);
        if (!shape.diagnostics.empty()) {
            return shape;
        }

        timeline_.markers.push_back({.tick = event.tick(),
                                     .sequence = event.sequence(),
                                     .type = event.type(),
                                     .task_id = event.entities().task_id,
                                     .job_id = event.entities().job_id,
                                     .resource_id = event.entities().resource_id,
                                     .message_id = event.entities().message_id,
                                     .cause_sequence = event.cause_sequence()});
        timeline_.marker_index.push_back(
            {.sequence = event.sequence(), .marker_index = timeline_.markers.size() - 1});

        auto failure = update_lifecycle(event, index);
        if (!failure.diagnostics.empty()) {
            return failure;
        }
        previous_tick_ = event.tick();
        previous_phase_ = event.phase();
        return {};
    }

    GuiTimelineBuildResult snapshot(Tick current_tick, std::size_t event_count) const {
        if (current_tick < 0) {
            return fail_current_tick(event_count, current_tick, "must not be negative");
        }
        if (previous_tick_.has_value() && *previous_tick_ > current_tick) {
            return fail_current_tick(event_count, current_tick,
                                     "precedes the last event tick " +
                                         std::to_string(*previous_tick_));
        }

        auto result = timeline_;
        result.current_tick = current_tick;
        for (const auto& [job, state] : jobs_) {
            if (state.state == DerivedJobState::Ready || state.state == DerivedJobState::Running) {
                append_open_interval(result.rows, job, state);
            }
        }
        for (auto& row : result.rows) {
            std::sort(row.intervals.begin(), row.intervals.end(), interval_less);
        }
        std::sort(
            result.marker_index.begin(), result.marker_index.end(),
            [](const GuiTimelineMarkerIndexEntry& left, const GuiTimelineMarkerIndexEntry& right) {
                return left.sequence < right.sequence;
            });
        return {.timeline = std::move(result), .diagnostics = {}};
    }

  private:
    GuiTimelineBuildResult update_lifecycle(const Event& event, std::size_t index) {
        if (event.type() == EventType::MessageDelivery) {
            return {};
        }

        const auto event_job_value = event_job(event);
        if (!event_job_value.has_value()) {
            throw std::logic_error{"validated timeline event has no job identity"};
        }
        const auto job = event_job_value.value();
        if (event.type() == EventType::MessageSend) {
            const auto source = jobs_.find(job);
            if (source == jobs_.end() || source->second.state != DerivedJobState::Completed) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "MessageSend requires completed source " + job_name(job));
            }
            return {};
        }

        const auto resource = event.entities().resource_id;
        if (!resource.has_value()) {
            throw std::logic_error{"validated timeline event has no resource identity"};
        }
        const auto resource_id = resource.value();
        auto found = jobs_.find(job);
        if (event.type() == EventType::JobRelease) {
            if (found != jobs_.end()) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "duplicate release for " + job_name(job));
            }
            jobs_.emplace(job, JobBuildState{.resource_id = resource_id,
                                             .state = DerivedJobState::Ready,
                                             .has_started = false,
                                             .interval_begin = event.tick(),
                                             .interval_begin_sequence = event.sequence(),
                                             .running_interval_resumed = false});
            return {};
        }
        if (found == jobs_.end()) {
            return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                        "event occurs before release for " + job_name(job));
        }
        auto& state = found->second;
        if (state.resource_id != resource_id) {
            return fail(GuiTimelineDiagnosticCode::ResourceMismatch, index, event,
                        job_name(job) + " expected R" + std::to_string(state.resource_id.value()) +
                            ", found R" + std::to_string(resource_id.value()));
        }
        auto& running = running_by_resource_.at(resource_id);

        if (event.type() == EventType::JobStart) {
            if (state.state != DerivedJobState::Ready || state.has_started) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "JobStart requires newly released Ready " + job_name(job));
            }
            if (running.has_value()) {
                return fail(GuiTimelineDiagnosticCode::OverlappingRunningIntervals, index, event,
                            "R" + std::to_string(resource_id.value()) + " already runs " +
                                job_name(running.value()) + " when " + job_name(job) + " starts");
            }
            const auto closed = append_interval(timeline_.rows, GuiTimelineIntervalKind::Ready, job,
                                                state, event.tick(), event.sequence());
            if (!closed.diagnostics.empty()) {
                return closed;
            }
            state.state = DerivedJobState::Running;
            state.has_started = true;
            state.interval_begin = event.tick();
            state.interval_begin_sequence = event.sequence();
            state.running_interval_resumed = false;
            running = job;
        } else if (event.type() == EventType::JobPreempt) {
            if (state.state != DerivedJobState::Running || running != job) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "JobPreempt requires Running " + job_name(job));
            }
            const auto closed = append_interval(timeline_.rows, GuiTimelineIntervalKind::Running,
                                                job, state, event.tick(), event.sequence());
            if (!closed.diagnostics.empty()) {
                return closed;
            }
            running.reset();
            state.state = DerivedJobState::Ready;
            state.interval_begin = event.tick();
            state.interval_begin_sequence = event.sequence();
            state.running_interval_resumed = false;
        } else if (event.type() == EventType::JobResume) {
            if (state.state != DerivedJobState::Ready || !state.has_started) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "JobResume requires previously preempted Ready " + job_name(job));
            }
            if (running.has_value()) {
                return fail(GuiTimelineDiagnosticCode::OverlappingRunningIntervals, index, event,
                            "R" + std::to_string(resource_id.value()) + " already runs " +
                                job_name(running.value()) + " when " + job_name(job) + " resumes");
            }
            const auto closed = append_interval(timeline_.rows, GuiTimelineIntervalKind::Ready, job,
                                                state, event.tick(), event.sequence());
            if (!closed.diagnostics.empty()) {
                return closed;
            }
            state.state = DerivedJobState::Running;
            state.interval_begin = event.tick();
            state.interval_begin_sequence = event.sequence();
            state.running_interval_resumed = true;
            running = job;
        } else if (event.type() == EventType::JobFinish) {
            if (state.state != DerivedJobState::Running || running != job) {
                return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                            "JobFinish requires Running " + job_name(job));
            }
            const auto closed = append_interval(timeline_.rows, GuiTimelineIntervalKind::Running,
                                                job, state, event.tick(), event.sequence());
            if (!closed.diagnostics.empty()) {
                return closed;
            }
            running.reset();
            state.state = DerivedJobState::Completed;
        } else if (event.type() == EventType::DeadlineMiss &&
                   state.state == DerivedJobState::Completed) {
            return fail(GuiTimelineDiagnosticCode::InvalidJobTransition, index, event,
                        "DeadlineMiss refers to completed " + job_name(job));
        }
        return {};
    }

    ExperimentPresentationSnapshot experiment_;
    GuiTimelineModel timeline_;
    std::map<JobIdentity, JobBuildState> jobs_;
    std::map<ResourceId, std::optional<JobIdentity>> running_by_resource_;
    std::set<EventSequence> observed_sequences_;
    std::optional<Tick> previous_tick_;
    std::optional<EventPhase> previous_phase_;
};

TimelineDerivation& require_derivation(std::optional<TimelineDerivation>& derivation) {
    if (!derivation.has_value()) {
        throw std::logic_error{"compatible timeline cache has no derivation"};
    }
    return derivation.value();
}

} // namespace

GuiTimelineBuildResult build_timeline_model(const std::vector<Event>& events,
                                            const ExperimentPresentationSnapshot& experiment,
                                            Tick current_tick) {
    if (current_tick < 0) {
        return fail_current_tick(events.size(), current_tick, "must not be negative");
    }
    TimelineDerivation derivation{experiment};
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto appended = derivation.append(events[index], index, current_tick);
        if (!appended.diagnostics.empty()) {
            return appended;
        }
    }
    return derivation.snapshot(current_tick, events.size());
}

struct GuiTimelineCache::State {
    std::optional<ExperimentPresentationSnapshot> experiment;
    std::optional<TimelineDerivation> derivation;
    std::size_t event_count{0};
    std::optional<Event> boundary_event;
    GuiTimelineBuildResult result;
};

GuiTimelineCache::GuiTimelineCache() : state_{std::make_unique<State>()} {}
GuiTimelineCache::~GuiTimelineCache() = default;
GuiTimelineCache::GuiTimelineCache(GuiTimelineCache&&) noexcept = default;
GuiTimelineCache& GuiTimelineCache::operator=(GuiTimelineCache&&) noexcept = default;

/*** Discards all presentation derivation so the next update rebuilds. ***/
void GuiTimelineCache::clear() { state_ = std::make_unique<State>(); }

/*** Applies only the unseen suffix when the canonical trace boundary is unchanged. ***/
const GuiTimelineBuildResult&
GuiTimelineCache::update(const std::vector<Event>& events,
                         const ExperimentPresentationSnapshot& experiment, Tick current_tick) {
    if (current_tick < 0) {
        state_->result = fail_current_tick(events.size(), current_tick, "must not be negative");
        return state_->result;
    }
    const auto& cached_experiment = state_->experiment;
    const auto& boundary_event = state_->boundary_event;
    auto prefix_compatible = cached_experiment.has_value() && state_->derivation.has_value() &&
                             events.size() >= state_->event_count;
    if (prefix_compatible) {
        if (cached_experiment.has_value()) {
            prefix_compatible = cached_experiment.value() == experiment;
        } else {
            prefix_compatible = false;
        }
    }
    if (prefix_compatible && state_->event_count > 0) {
        if (boundary_event.has_value()) {
            prefix_compatible = same_event(events[state_->event_count - 1], boundary_event.value());
        } else {
            prefix_compatible = false;
        }
    }

    if (!prefix_compatible) {
        auto rebuilt = std::make_unique<State>();
        rebuilt->experiment = experiment;
        TimelineDerivation derivation{experiment};
        for (std::size_t index = 0; index < events.size(); ++index) {
            const auto appended = derivation.append(events[index], index, current_tick);
            if (!appended.diagnostics.empty()) {
                state_->result = appended;
                return state_->result;
            }
        }
        rebuilt->event_count = events.size();
        if (!events.empty()) {
            rebuilt->boundary_event = events.back();
        }
        rebuilt->result = derivation.snapshot(current_tick, events.size());
        rebuilt->derivation = std::move(derivation);
        state_ = std::move(rebuilt);
        return state_->result;
    }

    auto& derivation = require_derivation(state_->derivation);
    if (events.size() > state_->event_count) {
        auto candidate = derivation;
        for (auto index = state_->event_count; index < events.size(); ++index) {
            const auto appended = candidate.append(events[index], index, current_tick);
            if (!appended.diagnostics.empty()) {
                state_->result = appended;
                return state_->result;
            }
        }
        auto snapshot = candidate.snapshot(current_tick, events.size());
        if (!snapshot.valid()) {
            state_->result = std::move(snapshot);
            return state_->result;
        }
        state_->derivation = std::move(candidate);
        state_->event_count = events.size();
        state_->boundary_event = events.back();
        state_->result = std::move(snapshot);
        return state_->result;
    }

    auto current_tick_valid = current_tick >= 0;
    if (current_tick_valid) {
        if (boundary_event.has_value()) {
            current_tick_valid = current_tick >= boundary_event.value().tick();
        }
    }
    auto& timeline = state_->result.timeline;
    if (current_tick_valid && timeline.has_value()) {
        timeline.value().current_tick = current_tick;
        state_->result.diagnostics.clear();
    } else {
        state_->result = derivation.snapshot(current_tick, state_->event_count);
    }
    return state_->result;
}

const GuiTimelineMarker* find_timeline_marker(const GuiTimelineModel& timeline,
                                              EventSequence sequence) {
    const auto found =
        std::lower_bound(timeline.marker_index.begin(), timeline.marker_index.end(), sequence,
                         [](const GuiTimelineMarkerIndexEntry& entry, EventSequence candidate) {
                             return entry.sequence < candidate;
                         });
    if (found == timeline.marker_index.end() || found->sequence != sequence ||
        found->marker_index >= timeline.markers.size()) {
        return nullptr;
    }
    return &timeline.markers[found->marker_index];
}

} // namespace cpssim
