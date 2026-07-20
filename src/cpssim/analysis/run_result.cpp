/*** Derive deterministic metrics from detached immutable simulation data. ***/

#include "cpssim/analysis/run_result.hpp"

#include "cpssim/model/time.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace cpssim {
namespace {

template <typename Key>
void add_sample(std::map<Key, TickStatistics>& statistics, Key key, Tick value) {
    auto [found, inserted] = statistics.try_emplace(key, TickStatistics{1, value, value, value});
    if (!inserted) {
        ++found->second.count;
        found->second.minimum = std::min(found->second.minimum, value);
        found->second.maximum = std::max(found->second.maximum, value);
        found->second.total += value;
    }
}

void add_sample(TickStatistics& statistics, Tick value) {
    if (statistics.count == 0) {
        statistics = {1, value, value, value};
        return;
    }
    ++statistics.count;
    statistics.minimum = std::min(statistics.minimum, value);
    statistics.maximum = std::max(statistics.maximum, value);
    statistics.total += value;
}

std::optional<JobIdentity> event_job(const Event& event) {
    const auto& entities = event.entities();
    if (!entities.task_id.has_value() || !entities.job_id.has_value()) {
        return std::nullopt;
    }
    return JobIdentity{*entities.task_id, *entities.job_id};
}

} // namespace

double TickStatistics::mean() const noexcept {
    return count == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(count);
}

std::string_view canonical_event_type_name(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return "job_release";
    case EventType::JobStart:
        return "job_start";
    case EventType::JobPreempt:
        return "job_preempt";
    case EventType::JobResume:
        return "job_resume";
    case EventType::JobFinish:
        return "job_finish";
    case EventType::DeadlineMiss:
        return "deadline_miss";
    case EventType::MessageSend:
        return "message_send";
    case EventType::MessageDelivery:
        return "message_delivery";
    }
    throw std::logic_error{"unsupported event type"};
}

std::string_view canonical_event_phase_name(EventPhase phase) {
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
    }
    throw std::logic_error{"unsupported event phase"};
}

RunMetrics derive_run_metrics(const SimulationSnapshot& snapshot) {
    RunMetrics result;
    result.event_count = snapshot.event_log.size();
    result.tick_period = snapshot.experiment.tick_period;
    result.horizon_tick = snapshot.current_tick;
    try {
        result.horizon_time =
            ticks_to_duration(snapshot.current_tick, snapshot.experiment.tick_period);
    } catch (const std::invalid_argument&) {
        result.horizon_time = std::nullopt;
    } catch (const std::overflow_error&) {
        result.horizon_time = std::nullopt;
    }

    std::map<JobIdentity, Tick> releases;
    std::map<TaskId, TickStatistics> responses;
    std::map<TaskId, std::uint64_t> task_completions;
    std::map<TaskId, std::uint64_t> task_deadline_misses;
    std::map<MessageId, Tick> sends;
    TickStatistics message_delays;

    for (const auto& event : snapshot.event_log) {
        switch (event.type()) {
        case EventType::JobRelease:
            if (const auto job = event_job(event); job.has_value()) {
                releases.try_emplace(*job, event.tick());
            }
            break;
        case EventType::JobFinish:
            ++result.completed_jobs;
            if (const auto job = event_job(event); job.has_value()) {
                ++task_completions[job->task_id()];
                if (const auto release = releases.find(*job);
                    release != releases.end() && event.tick() >= release->second) {
                    add_sample(responses, job->task_id(), event.tick() - release->second);
                }
            }
            break;
        case EventType::DeadlineMiss:
            ++result.deadline_misses;
            if (const auto task_id = event.entities().task_id; task_id.has_value()) {
                ++task_deadline_misses[task_id.value()];
            }
            break;
        case EventType::JobPreempt:
            ++result.preemptions;
            break;
        case EventType::MessageSend:
            ++result.messages.sent;
            if (const auto message_id = event.entities().message_id; message_id.has_value()) {
                sends.try_emplace(message_id.value(), event.tick());
            }
            break;
        case EventType::MessageDelivery:
            ++result.messages.delivered;
            if (const auto message_id = event.entities().message_id; message_id.has_value()) {
                if (const auto send = sends.find(message_id.value());
                    send != sends.end() && event.tick() >= send->second) {
                    add_sample(message_delays, event.tick() - send->second);
                }
            }
            break;
        case EventType::JobStart:
        case EventType::JobResume:
            break;
        }
    }

    if (message_delays.count != 0) {
        result.messages.delivery_delay = message_delays;
    }
    result.task_responses.reserve(snapshot.experiment.tasks.size());
    for (const auto& task : snapshot.experiment.tasks) {
        const auto found = responses.find(task.id);
        result.task_responses.push_back(
            {.task_id = task.id,
             .task_name = task.name,
             .deadline = task.deadline,
             .completed_jobs = task_completions[task.id],
             .deadline_misses = task_deadline_misses[task.id],
             .response_time = found == responses.end()
                                  ? std::optional<TickStatistics>{}
                                  : std::optional<TickStatistics>{found->second}});
    }
    result.resources.reserve(snapshot.resources.size());
    for (const auto& resource : snapshot.resources) {
        const auto observed = resource.busy_ticks + resource.idle_ticks;
        result.resources.push_back(
            {.resource_id = resource.id,
             .resource_name = resource.name,
             .busy_ticks = resource.busy_ticks,
             .idle_ticks = resource.idle_ticks,
             .utilization = observed > 0
                                ? std::optional<double>{static_cast<double>(resource.busy_ticks) /
                                                        static_cast<double>(observed)}
                                : std::nullopt});
    }
    std::sort(
        result.resources.begin(), result.resources.end(),
        [](const auto& left, const auto& right) { return left.resource_id < right.resource_id; });
    return result;
}

RunResult build_run_result(SimulationSnapshot snapshot, std::string scenario_kind) {
    auto signals =
        build_signal_model(snapshot.functional_observations, snapshot.functional_signal_registry);
    auto metrics = derive_run_metrics(snapshot);
    return {.scenario_kind = std::move(scenario_kind),
            .snapshot = std::move(snapshot),
            .signals = std::move(signals),
            .metrics = std::move(metrics)};
}

SimulationSnapshot select_run_result_range(const SimulationSnapshot& snapshot, Tick begin_tick,
                                           Tick end_tick) {
    if (begin_tick < 0 || end_tick < begin_tick) {
        throw std::invalid_argument{"result range must be nonnegative and ordered"};
    }
    auto result = snapshot;
    std::erase_if(result.event_log, [begin_tick, end_tick](const Event& event) {
        return event.tick() < begin_tick || event.tick() > end_tick;
    });
    std::erase_if(result.functional_observations,
                  [begin_tick, end_tick](const FunctionalObservation& observation) {
                      return observation.tick < begin_tick || observation.tick > end_tick;
                  });
    result.current_tick = std::min(snapshot.current_tick, end_tick);
    for (auto& resource : result.resources) {
        resource.busy_ticks = 0;
        resource.idle_ticks = 0;
    }
    return result;
}

} // namespace cpssim
