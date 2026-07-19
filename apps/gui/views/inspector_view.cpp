/***
 * File: apps/gui/views/inspector_view.cpp
 * Purpose: Render read-only experiment, runtime, job, and event details for
 *          the shared G02 selection.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "inspector_view.hpp"

#include "cpssim/trace/event_json.hpp"

#include "imgui.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace cpssim::gui {
namespace {

/*** Returns the stable display spelling for experiment preemption behavior. ***/
const char* preemption_name(PreemptionMode mode) {
    switch (mode) {
    case PreemptionMode::Preemptive:
        return "Preemptive";
    case PreemptionMode::NonPreemptive:
        return "Non-preemptive";
    }
    throw std::logic_error{"unknown preemption mode"};
}

/*** Returns the current controller state spelling. ***/
const char* run_state_name(GuiRunState state) {
    switch (state) {
    case GuiRunState::NotConfigured:
        return "Not configured";
    case GuiRunState::Paused:
        return "Paused";
    case GuiRunState::Running:
        return "Running";
    case GuiRunState::Finished:
        return "Finished";
    }
    throw std::logic_error{"unknown GUI run state"};
}

/*** Returns the canonical event category spelling used by the Inspector. ***/
const char* event_type_name(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return "Job release";
    case EventType::JobStart:
        return "Job start";
    case EventType::JobPreempt:
        return "Job preempt";
    case EventType::JobResume:
        return "Job resume";
    case EventType::JobFinish:
        return "Job finish";
    case EventType::DeadlineMiss:
        return "Deadline miss";
    case EventType::MessageSend:
        return "Message send";
    case EventType::MessageDelivery:
        return "Message delivery";
    }
    throw std::logic_error{"unknown event type"};
}

/*** Returns the deterministic processing-phase spelling for one event. ***/
const char* event_phase_name(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return "Execution completion";
    case EventPhase::MessageDelivery:
        return "Message delivery";
    case EventPhase::DeadlineCheck:
        return "Deadline check";
    case EventPhase::JobRelease:
        return "Job release";
    case EventPhase::PolicyUpdate:
        return "Policy update";
    case EventPhase::Scheduling:
        return "Scheduling";
    case EventPhase::CausedAction:
        return "Caused action";
    }
    throw std::logic_error{"unknown event phase"};
}

/*** Renders an optional strong numeric reference or explicit unavailability. ***/
template <typename Identifier>
void draw_optional_id(const char* label, const std::optional<Identifier>& identifier) {
    if (identifier.has_value()) {
        ImGui::Text("%s: %llu", label, static_cast<unsigned long long>(identifier->value()));
    } else {
        ImGui::TextDisabled("%s: Unavailable", label);
    }
}

/*** Draws general immutable experiment and current run information. ***/
void draw_experiment(const SimulationSnapshot& snapshot) {
    ImGui::SeparatorText("Experiment");
    ImGui::Text("Tick duration: %lld ns",
                static_cast<long long>(snapshot.experiment.tick_period.count()));
    ImGui::Text("Scheduling: %s", preemption_name(snapshot.experiment.preemption_mode));
    ImGui::Text("Run state: %s", run_state_name(snapshot.run_state));
    ImGui::Text("Logical tick: %lld / %lld", static_cast<long long>(snapshot.current_tick),
                static_cast<long long>(snapshot.stop_tick));
    ImGui::Text("Tasks: %zu", snapshot.experiment.tasks.size());
    ImGui::Text("Resources: %zu", snapshot.experiment.resources.size());
    ImGui::Text("Profiles: %zu", snapshot.experiment.profiles.size());
    ImGui::Text("Message routes: %zu", snapshot.experiment.routes.size());
}

/*** Draws immutable timing, profiles, and applied assignment for one task. ***/
void draw_task(const SimulationSnapshot& snapshot, TaskId task_id) {
    const auto* task = find_task(snapshot.experiment, task_id);
    if (task == nullptr) {
        ImGui::TextDisabled("Selected task is unavailable");
        return;
    }

    ImGui::SeparatorText("Task");
    ImGui::Text("Name: %s", task->name.c_str());
    ImGui::Text("Task ID: %llu", static_cast<unsigned long long>(task->id.value()));
    ImGui::Text("Priority: %d", task->priority);
    ImGui::Text("Period: %lld ticks", static_cast<long long>(task->period));
    ImGui::Text("Deadline: %lld ticks", static_cast<long long>(task->deadline));
    ImGui::Text("Offset: %lld ticks", static_cast<long long>(task->offset));

    ImGui::SeparatorText("Execution profiles");
    for (const auto& profile : snapshot.experiment.profiles) {
        if (profile.task_id != task_id) {
            continue;
        }
        const auto* resource = find_resource(snapshot.experiment, profile.resource_id);
        const auto name = resource != nullptr ? resource->name.c_str() : "Unavailable";
        ImGui::BulletText("%s (R%llu): %lld ticks", name,
                          static_cast<unsigned long long>(profile.resource_id.value()),
                          static_cast<long long>(profile.execution_time));
    }

    ImGui::SeparatorText("Applied assignment");
    const auto* assignment = find_assignment(snapshot.experiment, task_id);
    if (assignment == nullptr) {
        ImGui::TextDisabled("Unavailable");
        return;
    }
    const auto* resource = find_resource(snapshot.experiment, assignment->resource_id);
    ImGui::Text("%s (R%llu)", resource != nullptr ? resource->name.c_str() : "Unavailable",
                static_cast<unsigned long long>(assignment->resource_id.value()));
}

/*** Finds copied current runtime state for one resource. ***/
const GuiResourceSnapshot* find_runtime_resource(const SimulationSnapshot& snapshot,
                                                 ResourceId resource_id) {
    const auto found = std::find_if(
        snapshot.resources.begin(), snapshot.resources.end(),
        [resource_id](const GuiResourceSnapshot& resource) { return resource.id == resource_id; });
    return found != snapshot.resources.end() ? &*found : nullptr;
}

/*** Formats one complete job identity. ***/
std::string job_name(JobIdentity job) {
    return "T" + std::to_string(job.task_id().value()) + ":J" +
           std::to_string(job.job_id().value());
}

/*** Draws immutable assignment and detached runtime state for one resource. ***/
void draw_resource(const SimulationSnapshot& snapshot, ResourceId resource_id) {
    const auto* resource = find_resource(snapshot.experiment, resource_id);
    if (resource == nullptr) {
        ImGui::TextDisabled("Selected resource is unavailable");
        return;
    }

    ImGui::SeparatorText("Resource");
    ImGui::Text("Name: %s", resource->name.c_str());
    ImGui::Text("Resource ID: %llu", static_cast<unsigned long long>(resource->id.value()));

    ImGui::SeparatorText("Assigned tasks");
    bool has_assignment = false;
    for (const auto& assignment : snapshot.experiment.assignments) {
        if (assignment.resource_id != resource_id) {
            continue;
        }
        has_assignment = true;
        const auto* task = find_task(snapshot.experiment, assignment.task_id);
        ImGui::BulletText("%s (T%llu)", task != nullptr ? task->name.c_str() : "Unavailable",
                          static_cast<unsigned long long>(assignment.task_id.value()));
    }
    if (!has_assignment) {
        ImGui::TextDisabled("None");
    }

    ImGui::SeparatorText("Runtime");
    const auto* runtime = find_runtime_resource(snapshot, resource_id);
    if (runtime == nullptr) {
        ImGui::TextDisabled("Unavailable");
        return;
    }
    if (runtime->running_job.has_value()) {
        const auto running = job_name(*runtime->running_job);
        ImGui::Text("Running: %s", running.c_str());
    } else {
        ImGui::Text("Running: Idle");
    }
    ImGui::Text("Ready jobs: %zu", runtime->ready_jobs.size());
    for (const auto& job : runtime->ready_jobs) {
        const auto name = job_name(job);
        ImGui::BulletText("%s", name.c_str());
    }
    ImGui::Text("Busy ticks: %lld", static_cast<long long>(runtime->busy_ticks));
    ImGui::Text("Idle ticks: %lld", static_cast<long long>(runtime->idle_ticks));
}

/*** Draws one configured fixed-delay route. ***/
void draw_route(const SimulationSnapshot& snapshot, GuiRouteIdentity route_id) {
    const auto* route = find_route(snapshot.experiment, route_id);
    if (route == nullptr) {
        ImGui::TextDisabled("Selected route is unavailable");
        return;
    }

    ImGui::SeparatorText("Message route");
    const auto* source = find_task(snapshot.experiment, route->identity.source_task_id);
    const auto* destination = find_task(snapshot.experiment, route->identity.destination_task_id);
    ImGui::Text("Source task: %s (T%llu)", source != nullptr ? source->name.c_str() : "Unavailable",
                static_cast<unsigned long long>(route->identity.source_task_id.value()));
    ImGui::Text("Destination task: %s (T%llu)",
                destination != nullptr ? destination->name.c_str() : "Unavailable",
                static_cast<unsigned long long>(route->identity.destination_task_id.value()));
    ImGui::Text("Send offset: %lld ticks", static_cast<long long>(route->send_offset));
    ImGui::Text("Delay: %lld ticks", static_cast<long long>(route->delay));
}

/*** Draws current runtime membership or explicit trace-only status for a job. ***/
void draw_job(const SimulationSnapshot& snapshot, JobIdentity selected_job) {
    ImGui::SeparatorText("Job");
    ImGui::Text("Task ID: %llu", static_cast<unsigned long long>(selected_job.task_id().value()));
    ImGui::Text("Job ID: %llu", static_cast<unsigned long long>(selected_job.job_id().value()));

    for (const auto& resource : snapshot.resources) {
        if (resource.running_job == selected_job) {
            ImGui::Text("Runtime state: Running on R%llu",
                        static_cast<unsigned long long>(resource.id.value()));
            return;
        }
        if (std::find(resource.ready_jobs.begin(), resource.ready_jobs.end(), selected_job) !=
            resource.ready_jobs.end()) {
            ImGui::Text("Runtime state: Ready on R%llu",
                        static_cast<unsigned long long>(resource.id.value()));
            return;
        }
    }
    ImGui::TextDisabled("Runtime state: Unavailable (trace reference only)");
}

/*** Finds one canonical event by stable sequence. ***/
const Event* find_event(const SimulationSnapshot& snapshot, EventSequence sequence) {
    const auto found =
        std::find_if(snapshot.event_log.begin(), snapshot.event_log.end(),
                     [sequence](const Event& event) { return event.sequence() == sequence; });
    return found != snapshot.event_log.end() ? &*found : nullptr;
}

/*** Draws canonical event fields and permits selecting its complete job ref. ***/
void draw_event(const SimulationSnapshot& snapshot, EventSequence sequence,
                GuiSelection& selection) {
    const auto* event = find_event(snapshot, sequence);
    if (event == nullptr) {
        ImGui::TextDisabled("Selected event is unavailable");
        return;
    }

    ImGui::SeparatorText("Canonical event");
    ImGui::Text("Sequence: %llu", static_cast<unsigned long long>(sequence.value()));
    ImGui::Text("Tick: %lld", static_cast<long long>(event->tick()));
    ImGui::Text("Type: %s", event_type_name(event->type()));
    ImGui::Text("Phase: %s", event_phase_name(event->phase()));
    draw_optional_id("Task ID", event->entities().task_id);
    draw_optional_id("Job ID", event->entities().job_id);
    draw_optional_id("Resource ID", event->entities().resource_id);
    draw_optional_id("Message ID", event->entities().message_id);
    draw_optional_id("Vehicle ID", event->entities().vehicle_id);
    draw_optional_id("Cause sequence", event->cause_sequence());

    if (event->entities().task_id.has_value() && event->entities().job_id.has_value() &&
        ImGui::Button("Select referenced job")) {
        selection.select_job(JobIdentity{*event->entities().task_id, *event->entities().job_id});
    }

    ImGui::SeparatorText("Canonical JSON");
    const auto json = serialize_event_json_line(*event);
    ImGui::TextWrapped("%.*s", static_cast<int>(json.size() - 1), json.data());
}

} // namespace

/*** Dispatches the shared selection to a read-only detail renderer. ***/
void draw_inspector_view(const SimulationSnapshot& snapshot, GuiSelection& selection) {
    if (const auto range = selection.tick_range(); range.has_value()) {
        ImGui::SeparatorText("Time selection");
        if (range->begin_tick == range->end_tick) {
            ImGui::Text("Tick: %lld", static_cast<long long>(range->begin_tick));
        } else {
            ImGui::Text("Ticks: %lld to %lld (inclusive selection)",
                        static_cast<long long>(range->begin_tick),
                        static_cast<long long>(range->end_tick));
        }
    }
    switch (selection.kind()) {
    case GuiSelectionKind::None:
        ImGui::TextDisabled("Select an experiment entity, resource job, or event.");
        break;
    case GuiSelectionKind::Experiment:
        draw_experiment(snapshot);
        break;
    case GuiSelectionKind::Task:
        draw_task(snapshot, *selection.task_id());
        break;
    case GuiSelectionKind::Resource:
        draw_resource(snapshot, *selection.resource_id());
        break;
    case GuiSelectionKind::Route:
        draw_route(snapshot, *selection.route_id());
        break;
    case GuiSelectionKind::Job:
        draw_job(snapshot, *selection.job());
        break;
    case GuiSelectionKind::Event:
        draw_event(snapshot, *selection.event_sequence(), selection);
        break;
    }
}

} // namespace cpssim::gui
