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

bool begin_property_grid(const char* identity) {
    if (!ImGui::BeginTable(identity, 2,
                           ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
        return false;
    }
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                            9.0F * ImGui::GetFontSize());
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void draw_property(const char* label, const std::string& value, bool unavailable = false) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    if (unavailable) {
        ImGui::TextDisabled("%s", value.c_str());
    } else {
        ImGui::TextUnformatted(value.c_str());
    }
    if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted(value.c_str());
        ImGui::EndTooltip();
    }
}

template <typename Identifier>
std::string optional_id_text(const std::optional<Identifier>& identifier) {
    return identifier.has_value() ? std::to_string(identifier->value()) : "Unavailable";
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
    if (begin_property_grid("Runtime resource properties")) {
        draw_property("Resource", std::to_string(resource_id.value()));
        draw_property("Running",
                      runtime->running_job.has_value() ? job_label(*runtime->running_job) : "Idle");
        draw_property("Ready", std::to_string(runtime->ready_jobs.size()));
        draw_property("Busy ticks", std::to_string(runtime->busy_ticks));
        draw_property("Idle ticks", std::to_string(runtime->idle_ticks));
        const auto total = runtime->busy_ticks + runtime->idle_ticks;
        const auto utilization = total > 0 ? 100.0 * static_cast<double>(runtime->busy_ticks) /
                                                 static_cast<double>(total)
                                           : 0.0;
        draw_property("Utilization", std::to_string(utilization).substr(0, 5) + "%");
        ImGui::EndTable();
    }
    for (const auto& job : runtime->ready_jobs) {
        ImGui::BulletText("%s", job_label(job).c_str());
    }
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
    if (begin_property_grid("Runtime job properties")) {
        draw_property("Task", std::to_string(selected.task_id().value()));
        draw_property("Job", std::to_string(selected.job_id().value()));
        draw_property("Lifecycle", lifecycle);
        const auto tick_text = [](std::optional<Tick> value) {
            return value.has_value() ? std::to_string(*value) : "Unavailable";
        };
        draw_property("Release tick", tick_text(release), !release.has_value());
        draw_property("Start tick", tick_text(start), !start.has_value());
        draw_property("Finish tick", tick_text(finish), !finish.has_value());
        draw_property("Deadline", tick_text(deadline), !deadline.has_value());
        const auto response = release.has_value() && finish.has_value()
                                  ? std::to_string(*finish - *release) + " ticks"
                                  : "Unavailable";
        draw_property("Response time", response, !release.has_value() || !finish.has_value());
        draw_property("Resource", optional_id_text(assigned_resource),
                      !assigned_resource.has_value());
        ImGui::EndTable();
    }
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
    const auto physical_ms = static_cast<double>(event->tick()) *
                             static_cast<double>(snapshot.experiment.tick_period.count()) /
                             1'000'000.0;
    if (begin_property_grid("Canonical event properties")) {
        draw_property("Sequence", std::to_string(sequence.value()));
        draw_property("Tick", std::to_string(event->tick()));
        auto time_text = std::to_string(physical_ms);
        time_text.resize(std::min<std::size_t>(time_text.size(), 8));
        draw_property("Time", time_text + " ms");
        draw_property("Type", event_type_name(event->type()));
        draw_property("Phase", event_phase_name(event->phase()));
        draw_property("Task", optional_id_text(event->entities().task_id),
                      !event->entities().task_id.has_value());
        draw_property("Job", optional_id_text(event->entities().job_id),
                      !event->entities().job_id.has_value());
        draw_property("Resource", optional_id_text(event->entities().resource_id),
                      !event->entities().resource_id.has_value());
        draw_property("Message", optional_id_text(event->entities().message_id),
                      !event->entities().message_id.has_value());
        draw_property("Vehicle", optional_id_text(event->entities().vehicle_id),
                      !event->entities().vehicle_id.has_value());
        draw_property("Cause", optional_id_text(event->cause_sequence()),
                      !event->cause_sequence().has_value());
        ImGui::EndTable();
    }
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
        if (begin_property_grid("Timeline selection properties")) {
            draw_property("Ticks", std::to_string(range->begin_tick) + " to " +
                                       std::to_string(range->end_tick));
            ImGui::EndTable();
        }
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
