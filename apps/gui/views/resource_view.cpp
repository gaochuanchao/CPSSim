/***
 * File: apps/gui/views/resource_view.cpp
 * Purpose: Render copied resource state and a normalized busy-time histogram.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "resource_view.hpp"

#include "cpssim/gui/resource_presentation.hpp"

#include "imgui.h"

#include <string>
#include <vector>

namespace cpssim::gui {
namespace {

/*** Formats a complete task-local job identity for resource tables. ***/
std::string job_name(const JobIdentity& identity) {
    return "T" + std::to_string(identity.task_id().value()) + ":J" +
           std::to_string(identity.job_id().value());
}

} // namespace

/*** Draws copied resource rows with strong-ID resource and job selection. ***/
void draw_resource_state(const SimulationSnapshot& snapshot, GuiSelection& selection) {
    if (ImGui::BeginTable("resources", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Running");
        ImGui::TableSetupColumn("Ready");
        ImGui::TableSetupColumn("Busy ticks");
        ImGui::TableSetupColumn("Idle ticks");
        ImGui::TableSetupColumn("Utilization");
        ImGui::TableHeadersRow();

        for (const auto& resource : snapshot.resources) {
            const auto* task_assignment =
                selection.task_id().has_value()
                    ? find_assignment(snapshot.experiment, *selection.task_id())
                    : nullptr;
            const auto highlights_assignment =
                task_assignment != nullptr && task_assignment->resource_id == resource.id;
            const auto selected_resource = selection.resource_id() == resource.id;
            ImGui::TableNextRow();
            if (selected_resource || highlights_assignment) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImGuiCol_Header));
            }
            ImGui::TableSetColumnIndex(0);
            const auto resource_label =
                resource.name + " (" + std::to_string(resource.id.value()) + ')';
            const auto resource_stable_id = "resource:" + std::to_string(resource.id.value());
            ImGui::PushID(resource_stable_id.c_str());
            if (ImGui::Selectable(resource_label.c_str(), selected_resource)) {
                selection.select_resource(resource.id);
            }
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            const auto running = resource.running_job.has_value() ? job_name(*resource.running_job)
                                                                  : std::string{"Idle"};
            if (resource.running_job.has_value()) {
                const auto job_stable_id = "running:" + running;
                ImGui::PushID(job_stable_id.c_str());
                if (ImGui::Selectable(running.c_str(), selection.job() == resource.running_job)) {
                    selection.select_job(*resource.running_job);
                }
                ImGui::PopID();
            } else {
                ImGui::TextUnformatted(running.c_str());
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu", resource.ready_jobs.size());
            if (!resource.ready_jobs.empty() && ImGui::BeginItemTooltip()) {
                ImGui::TextUnformatted("Ready jobs");
                for (const auto& job : resource.ready_jobs) {
                    const auto name = job_name(job);
                    ImGui::BulletText("%s", name.c_str());
                }
                ImGui::EndTooltip();
            }
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%lld", static_cast<long long>(resource.busy_ticks));
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%lld", static_cast<long long>(resource.idle_ticks));
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.1f%%", 100.0 * calculate_resource_utilization(resource.busy_ticks,
                                                                         resource.idle_ticks));
        }
        ImGui::EndTable();
    }
}

void draw_utilization(const SimulationSnapshot& snapshot, GuiSelection& selection) {
    const auto rows = build_resource_presentation(snapshot);
    if (rows.empty()) {
        ImGui::TextDisabled("No resources are available.");
        return;
    }
    for (const auto& row : rows) {
        ImGui::PushID(static_cast<int>(row.id.value()));
        const auto selected = selection.resource_id() == row.id;
        if (ImGui::Selectable(
                row.name.c_str(), selected, ImGuiSelectableFlags_None,
                ImVec2{std::min(16.0F * ImGui::GetFontSize(), ImGui::GetContentRegionAvail().x),
                       0.0F})) {
            selection.select_resource(row.id);
        }
        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        const auto overlay = std::to_string(static_cast<int>(row.utilization * 100.0 + 0.5)) + "%";
        ImGui::ProgressBar(static_cast<float>(row.utilization), ImVec2{-1.0F, 0.0F},
                           overlay.c_str());
        ImGui::PopID();
    }
}

void draw_resource_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                        GuiResourceTab& active_tab, ResourceViewState& state) {
    if (!ImGui::BeginTabBar("Resource views")) {
        return;
    }
    const auto state_flags = state.restore_active_tab && active_tab == GuiResourceTab::ResourceState
                                 ? ImGuiTabItemFlags_SetSelected
                                 : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem("Resource State", nullptr, state_flags)) {
        active_tab = GuiResourceTab::ResourceState;
        draw_resource_state(snapshot, selection);
        ImGui::EndTabItem();
    }
    const auto utilization_flags =
        state.restore_active_tab && active_tab == GuiResourceTab::Utilization
            ? ImGuiTabItemFlags_SetSelected
            : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem("Utilization", nullptr, utilization_flags)) {
        active_tab = GuiResourceTab::Utilization;
        draw_utilization(snapshot, selection);
        ImGui::EndTabItem();
    }
    state.restore_active_tab = false;
    ImGui::EndTabBar();
}

} // namespace cpssim::gui
