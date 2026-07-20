/*** Render run-plan settings separately from structural system properties. ***/

#include "run_plan_editor.hpp"

#include "imgui.h"

#include <algorithm>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

constexpr ImVec4 error_color{1.0F, 0.55F, 0.35F, 1.0F};

std::string resource_label(const EditableSystemDraft& draft, ResourceId id) {
    const auto found = std::find_if(draft.resources().begin(), draft.resources().end(),
                                    [id](const auto& resource) { return resource.id == id; });
    const auto name = found == draft.resources().end() ? std::string{"Unknown"} : found->name;
    return name + " (R" + std::to_string(id.value()) + ')';
}

void draw_plan_diagnostics(const GuiSimulationSession& session) {
    if (!session.last_validation().has_value()) {
        return;
    }
    for (const auto& diagnostic : session.last_validation()->diagnostics) {
        ImGui::TextColored(error_color, "%s", diagnostic.message.c_str());
    }
}

void draw_system_assignments(const EditableSystemDraft& draft,
                             std::vector<DraftTaskAssignment>& assignments) {
    ImGui::SeparatorText("Task assignments");
    for (auto& assignment : assignments) {
        const auto task =
            std::find_if(draft.tasks().begin(), draft.tasks().end(),
                         [&assignment](const auto& row) { return row.id == assignment.task_id; });
        if (task == draft.tasks().end()) {
            continue;
        }
        ImGui::PushID(static_cast<int>(assignment.task_id.value()));
        ImGui::TextUnformatted(task->name.c_str());
        const auto preview = assignment.resource_id.has_value()
                                 ? resource_label(draft, *assignment.resource_id)
                                 : std::string{"Select resource..."};
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##assignment", preview.c_str())) {
            if (ImGui::Selectable("Unassigned", !assignment.resource_id.has_value())) {
                assignment.resource_id.reset();
            }
            for (const auto& profile : draft.profiles()) {
                if (profile.task_id != assignment.task_id) {
                    continue;
                }
                const auto selected = assignment.resource_id == profile.resource_id;
                if (ImGui::Selectable(resource_label(draft, profile.resource_id).c_str(),
                                      selected)) {
                    assignment.resource_id = profile.resource_id;
                }
            }
            ImGui::EndCombo();
        }
        if (!assignment.resource_id.has_value()) {
            ImGui::TextColored(error_color, "A resource assignment is required.");
        }
        ImGui::PopID();
    }
}

void draw_session_assignments(GuiSimulationSession& session,
                              const ExperimentPresentationSnapshot& experiment) {
    ImGui::SeparatorText("Task assignments");
    for (const auto& task : experiment.tasks) {
        ImGui::PushID(static_cast<int>(task.id.value()));
        ImGui::TextUnformatted(task.name.c_str());
        auto selected = session.draft().assignment(task.id);
        auto preview = std::string{"Select resource..."};
        if (selected.has_value()) {
            const auto* resource = find_resource(experiment, *selected);
            if (resource != nullptr) {
                preview = resource->name + " (R" + std::to_string(resource->id.value()) + ')';
            }
        }
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##assignment", preview.c_str())) {
            for (const auto& profile : experiment.profiles) {
                if (profile.task_id != task.id) {
                    continue;
                }
                const auto* resource = find_resource(experiment, profile.resource_id);
                const auto label =
                    resource == nullptr
                        ? std::string{"Unavailable"}
                        : resource->name + " (R" + std::to_string(resource->id.value()) + ')';
                if (ImGui::Selectable(label.c_str(), selected == profile.resource_id)) {
                    session.set_draft_assignment(task.id, profile.resource_id);
                    selected = profile.resource_id;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
}

} // namespace

RunConfigurationAction draw_run_configuration(GuiSimulationSession& session,
                                              const SimulationSnapshot& snapshot,
                                              const EditableSystemDraft* system_draft,
                                              std::vector<DraftTaskAssignment>& system_assignments,
                                              bool system_changes_dirty) {
    ImGui::Text("State: %s", system_changes_dirty || session.draft_dirty() ? "Pending changes"
                                                                           : "Matches applied");
    ImGui::TextDisabled("Scheduling policy: Fixed priority");
    auto stop_tick = session.draft().stop_tick();
    ImGui::BeginDisabled(!session.draft_editable());
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputScalar("Stop tick", ImGuiDataType_S64, &stop_tick)) {
        session.set_draft_stop_tick(stop_tick);
    }
    if (system_draft != nullptr) {
        draw_system_assignments(*system_draft, system_assignments);
    } else {
        draw_session_assignments(session, snapshot.experiment);
    }
    ImGui::SeparatorText("Actions");
    auto action = RunConfigurationAction::None;
    if (ImGui::Button("Validate changes")) {
        action = RunConfigurationAction::ValidateChanges;
    }
    ImGui::SameLine();
    if (ImGui::Button(system_draft != nullptr ? "Apply and restart" : "Apply and reset")) {
        action = RunConfigurationAction::ApplyAndRestart;
    }
    ImGui::EndDisabled();
    if (!session.draft_editable()) {
        ImGui::TextWrapped("Pause or reset the active run before editing run configuration.");
    }
    draw_plan_diagnostics(session);
    return action;
}

} // namespace cpssim::gui
