/*** Render runtime-only details from detached simulation snapshots. ***/

#include "inspector_view.hpp"

#include "cpssim/trace/event_json.hpp"

#include "imgui.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

namespace cpssim::gui {
namespace {

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

template <typename Identifier>
void draw_optional_id(const char* label, const std::optional<Identifier>& identifier) {
    if (identifier.has_value()) {
        ImGui::Text("%s: %llu", label, static_cast<unsigned long long>(identifier->value()));
    } else {
        ImGui::TextDisabled("%s: Unavailable", label);
    }
}

const GuiResourceSnapshot* find_runtime_resource(const SimulationSnapshot& snapshot,
                                                 ResourceId resource_id) {
    const auto found =
        std::find_if(snapshot.resources.begin(), snapshot.resources.end(),
                     [resource_id](const auto& resource) { return resource.id == resource_id; });
    return found == snapshot.resources.end() ? nullptr : &*found;
}

std::string job_label(JobIdentity job) {
    return "T" + std::to_string(job.task_id().value()) + ":J" +
           std::to_string(job.job_id().value());
}

void draw_runtime_resource(const SimulationSnapshot& snapshot, ResourceId resource_id) {
    const auto* runtime = find_runtime_resource(snapshot, resource_id);
    if (runtime == nullptr) {
        ImGui::TextDisabled("Selected runtime resource is unavailable.");
        return;
    }
    ImGui::SeparatorText("Runtime resource");
    ImGui::Text("Resource ID: %llu", static_cast<unsigned long long>(resource_id.value()));
    if (runtime->running_job.has_value()) {
        ImGui::Text("Running job: %s", job_label(*runtime->running_job).c_str());
    } else {
        ImGui::Text("Running job: Idle");
    }
    ImGui::Text("Ready jobs: %zu", runtime->ready_jobs.size());
    for (const auto& job : runtime->ready_jobs) {
        ImGui::BulletText("%s", job_label(job).c_str());
    }
    ImGui::Text("Busy ticks: %lld", static_cast<long long>(runtime->busy_ticks));
    ImGui::Text("Idle ticks: %lld", static_cast<long long>(runtime->idle_ticks));
    const auto total = runtime->busy_ticks + runtime->idle_ticks;
    const auto utilization =
        total > 0 ? 100.0 * static_cast<double>(runtime->busy_ticks) / static_cast<double>(total)
                  : 0.0;
    ImGui::Text("Utilization: %.1f%%", utilization);
}

bool event_refers_to_job(const Event& event, JobIdentity job) {
    return event.entities().task_id == job.task_id() && event.entities().job_id == job.job_id();
}

void draw_job(const SimulationSnapshot& snapshot, JobIdentity selected) {
    std::optional<Tick> release;
    std::optional<Tick> start;
    std::optional<Tick> finish;
    std::optional<ResourceId> assigned_resource;
    auto deadline_missed = false;
    for (const auto& event : snapshot.event_log) {
        if (!event_refers_to_job(event, selected)) {
            continue;
        }
        if (event.entities().resource_id.has_value()) {
            assigned_resource = event.entities().resource_id;
        }
        switch (event.type()) {
        case EventType::JobRelease:
            release = event.tick();
            break;
        case EventType::JobStart:
            if (!start.has_value()) {
                start = event.tick();
            }
            break;
        case EventType::JobFinish:
            finish = event.tick();
            break;
        case EventType::DeadlineMiss:
            deadline_missed = true;
            break;
        case EventType::JobPreempt:
        case EventType::JobResume:
        case EventType::MessageSend:
        case EventType::MessageDelivery:
            break;
        }
    }
    const auto* task = find_task(snapshot.experiment, selected.task_id());
    std::optional<Tick> deadline;
    if (release.has_value() && task != nullptr) {
        deadline = *release + task->deadline;
    }
    auto lifecycle = "Trace reference only";
    if (finish.has_value()) {
        lifecycle = "Finished";
    } else if (deadline_missed) {
        lifecycle = "Deadline missed";
    } else {
        for (const auto& resource : snapshot.resources) {
            if (resource.running_job == selected) {
                lifecycle = "Running";
            } else if (std::find(resource.ready_jobs.begin(), resource.ready_jobs.end(),
                                 selected) != resource.ready_jobs.end()) {
                lifecycle = "Ready";
            }
        }
    }
    ImGui::SeparatorText("Runtime job");
    ImGui::Text("Task ID: %llu", static_cast<unsigned long long>(selected.task_id().value()));
    ImGui::Text("Job ID: %llu", static_cast<unsigned long long>(selected.job_id().value()));
    ImGui::Text("Lifecycle: %s", lifecycle);
    const auto draw_tick = [](const char* label, std::optional<Tick> value) {
        if (value.has_value()) {
            ImGui::Text("%s: %lld", label, static_cast<long long>(*value));
        } else {
            ImGui::TextDisabled("%s: Unavailable", label);
        }
    };
    draw_tick("Release tick", release);
    draw_tick("Start tick", start);
    draw_tick("Finish tick", finish);
    draw_tick("Deadline", deadline);
    if (release.has_value() && finish.has_value()) {
        ImGui::Text("Response time: %lld ticks", static_cast<long long>(*finish - *release));
    } else {
        ImGui::TextDisabled("Response time: Unavailable");
    }
    draw_optional_id("Assigned resource", assigned_resource);
}

const Event* find_event(const SimulationSnapshot& snapshot, EventSequence sequence) {
    const auto found =
        std::find_if(snapshot.event_log.begin(), snapshot.event_log.end(),
                     [sequence](const Event& event) { return event.sequence() == sequence; });
    return found == snapshot.event_log.end() ? nullptr : &*found;
}

void draw_event(const SimulationSnapshot& snapshot, EventSequence sequence,
                GuiSelection& selection) {
    const auto* event = find_event(snapshot, sequence);
    if (event == nullptr) {
        ImGui::TextDisabled("Selected event is unavailable.");
        return;
    }
    ImGui::SeparatorText("Canonical event");
    ImGui::Text("Sequence: %llu", static_cast<unsigned long long>(sequence.value()));
    ImGui::Text("Tick: %lld", static_cast<long long>(event->tick()));
    const auto physical_ms = static_cast<double>(event->tick()) *
                             static_cast<double>(snapshot.experiment.tick_period.count()) /
                             1'000'000.0;
    ImGui::Text("Physical time: %.6f ms", physical_ms);
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
        selection.select_job({*event->entities().task_id, *event->entities().job_id});
    }
    ImGui::SeparatorText("Raw event JSON");
    const auto json = serialize_event_json_line(*event);
    ImGui::TextWrapped("%.*s", static_cast<int>(json.size() - 1), json.data());
}

} // namespace

void draw_inspector_view(const SimulationSnapshot& snapshot, GuiSelection& selection) {
    if (const auto range = selection.tick_range(); range.has_value()) {
        ImGui::SeparatorText("Timeline selection");
        ImGui::Text("Ticks: %lld to %lld", static_cast<long long>(range->begin_tick),
                    static_cast<long long>(range->end_tick));
    }
    switch (selection.kind()) {
    case GuiSelectionKind::Resource:
        draw_runtime_resource(snapshot, *selection.resource_id());
        break;
    case GuiSelectionKind::Job:
        draw_job(snapshot, *selection.job());
        break;
    case GuiSelectionKind::Event:
        draw_event(snapshot, *selection.event_sequence(), selection);
        break;
    case GuiSelectionKind::None:
        ImGui::TextDisabled("No runtime item selected.");
        ImGui::TextWrapped("Select an event, job, timeline item, or runtime resource state.");
        break;
    case GuiSelectionKind::Experiment:
    case GuiSelectionKind::Task:
    case GuiSelectionKind::Route:
        ImGui::TextDisabled("No runtime details are available for this highlight.");
        ImGui::TextWrapped("Structural properties are edited in System Builder.");
        break;
    }
}

} // namespace cpssim::gui
