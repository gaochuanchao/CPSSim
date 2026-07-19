/***
 * File: apps/gui/views/run_plan_editor.cpp
 * Purpose: Render the G03 explicit run-plan editor over GUI-neutral session
 *          state and shared validation diagnostics.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "run_plan_editor.hpp"

#include "imgui.h"

#include <algorithm>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

std::string resource_label(const ExperimentPresentationSnapshot& experiment,
                           ResourceId resource_id) {
    const auto* resource = find_resource(experiment, resource_id);
    const auto name = resource != nullptr ? resource->name : std::string{"Unknown"};
    return name + " (R" + std::to_string(resource_id.value()) + ')';
}

const GuiTaskResourceProfilePresentation*
find_profile(const ExperimentPresentationSnapshot& experiment, TaskId task_id,
             ResourceId resource_id) {
    const auto found =
        std::find_if(experiment.profiles.begin(), experiment.profiles.end(),
                     [task_id, resource_id](const GuiTaskResourceProfilePresentation& profile) {
                         return profile.task_id == task_id && profile.resource_id == resource_id;
                     });
    return found != experiment.profiles.end() ? &*found : nullptr;
}

std::optional<ResourceId> active_assignment(const RunPlan* plan, TaskId task_id) {
    if (plan == nullptr) {
        return std::nullopt;
    }
    const auto found = std::find_if(
        plan->assignments().begin(), plan->assignments().end(),
        [task_id](const TaskAssignment& assignment) { return assignment.task_id == task_id; });
    return found != plan->assignments().end() ? std::optional<ResourceId>{found->resource_id}
                                              : std::nullopt;
}

void draw_task_diagnostics(const GuiSimulationSession& session, TaskId task_id) {
    if (!session.last_validation().has_value()) {
        return;
    }
    for (const auto& diagnostic : session.last_validation()->diagnostics) {
        if (diagnostic.task_id == task_id) {
            ImGui::TextColored(ImVec4{1.0F, 0.55F, 0.35F, 1.0F}, "%s", diagnostic.message.c_str());
        }
    }
}

void draw_stop_tick_diagnostics(const GuiSimulationSession& session) {
    if (!session.last_validation().has_value()) {
        return;
    }
    for (const auto& diagnostic : session.last_validation()->diagnostics) {
        if (diagnostic.code == RunPlanDiagnosticCode::InvalidStopTick ||
            diagnostic.code == RunPlanDiagnosticCode::UnsupportedPolicy ||
            diagnostic.code == RunPlanDiagnosticCode::RunConstructionFailed) {
            ImGui::TextColored(ImVec4{1.0F, 0.55F, 0.35F, 1.0F}, "%s", diagnostic.message.c_str());
        }
    }
}

void draw_assignment_editor(GuiSimulationSession& session,
                            const ExperimentPresentationSnapshot& experiment,
                            const GuiTaskPresentation& task) {
    const auto editable = session.draft_editable();
    auto selected = session.draft().assignment(task.id);
    const auto preview = selected.has_value() ? resource_label(experiment, *selected)
                                              : std::string{"Select resource..."};
    const auto stable_id = "task-plan-" + std::to_string(task.id.value());
    ImGui::PushID(stable_id.c_str());
    ImGui::BeginDisabled(!editable);
    if (ImGui::BeginCombo("Draft resource", preview.c_str())) {
        if (ImGui::Selectable("Unassigned", !selected.has_value())) {
            session.set_draft_assignment(task.id, std::nullopt);
            selected.reset();
        }
        for (const auto& profile : experiment.profiles) {
            if (profile.task_id != task.id) {
                continue;
            }
            const auto label = resource_label(experiment, profile.resource_id);
            const auto is_selected = selected == profile.resource_id;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                session.set_draft_assignment(task.id, profile.resource_id);
                selected = profile.resource_id;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    if (selected.has_value()) {
        const auto* profile = find_profile(experiment, task.id, *selected);
        if (profile != nullptr) {
            ImGui::Text("Draft execution: %lld ticks",
                        static_cast<long long>(profile->execution_time));
        }
    } else {
        ImGui::TextDisabled("Draft execution: unavailable");
    }

    const auto active = active_assignment(session.active_plan(), task.id);
    if (active.has_value()) {
        const auto label = resource_label(experiment, *active);
        ImGui::TextDisabled("Active: %s", label.c_str());
    } else {
        ImGui::TextDisabled("Active: none");
    }
    draw_task_diagnostics(session, task.id);
    ImGui::PopID();
}

} // namespace

void draw_run_plan_editor(GuiSimulationSession& session, const SimulationSnapshot& snapshot) {
    ImGui::SeparatorText("Run plan");
    ImGui::Text("Draft: %s", session.draft_dirty() ? "Modified" : "Matches active");
    if (session.active_plan() != nullptr) {
        ImGui::TextDisabled("Active stop tick: %lld",
                            static_cast<long long>(session.active_plan()->stop_tick()));
    } else {
        ImGui::TextDisabled("Active plan: none");
    }
    ImGui::TextDisabled("Policy: Fixed priority");

    auto stop_tick = session.draft().stop_tick();
    ImGui::BeginDisabled(!session.draft_editable());
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputScalar("Draft stop tick", ImGuiDataType_S64, &stop_tick)) {
        session.set_draft_stop_tick(stop_tick);
    }
    ImGui::EndDisabled();
    draw_stop_tick_diagnostics(session);

    for (const auto& task : snapshot.experiment.tasks) {
        ImGui::SeparatorText(task.name.c_str());
        ImGui::TextDisabled("Task T%llu", static_cast<unsigned long long>(task.id.value()));
        draw_assignment_editor(session, snapshot.experiment, task);
    }

    if (!session.draft_editable()) {
        ImGui::TextWrapped("Pause the active run before editing or applying the draft.");
    }
    ImGui::BeginDisabled(!session.draft_editable());
    if (ImGui::Button("Validate")) {
        session.validate_draft();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply and reset")) {
        session.apply_draft();
    }
    ImGui::EndDisabled();

    if (session.last_validation().has_value()) {
        if (session.last_validation()->valid()) {
            ImGui::TextColored(ImVec4{0.45F, 0.85F, 0.55F, 1.0F}, "Plan is valid");
        } else {
            ImGui::TextColored(ImVec4{1.0F, 0.55F, 0.35F, 1.0F}, "%zu issue(s)",
                               session.last_validation()->diagnostics.size());
        }
    } else {
        ImGui::TextDisabled("Draft changed; validate or apply it.");
    }
}

} // namespace cpssim::gui
