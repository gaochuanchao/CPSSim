/***
 * File: apps/gui/gui_application.cpp
 * Purpose: Render the fixed, resize-aware CPSSim workbench shell.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Docking and persistent layouts remain outside G02. Panel visibility
 *        and strong-ID selection are owned by GuiApplication.
 ***/

#include "gui_application.hpp"

#include "views/architecture_view.hpp"
#include "views/event_view.hpp"
#include "views/experiment_explorer.hpp"
#include "views/inspector_view.hpp"
#include "views/resource_view.hpp"
#include "views/run_plan_editor.hpp"
#include "views/signal_view.hpp"
#include "views/timeline_view.hpp"
#include "views/toolbar_view.hpp"

#include "cpssim/config/json_run_plan.hpp"
#include "cpssim/core/version.hpp"

#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace cpssim::gui {

/*** Captures the command boundary and selects the experiment root initially. ***/
GuiApplication::GuiApplication(GuiSimulationSession& session) : session_{session} {
    selection_.select_experiment();
    constexpr std::string_view default_plan_path{"run-plan.json"};
    std::copy(default_plan_path.begin(), default_plan_path.end(), run_plan_path_.begin());
}

/*** Updates menu-owned visibility and opens the static About dialog. ***/
void GuiApplication::draw_main_menu() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        ImGui::BeginDisabled();
        ImGui::MenuItem("Open experiment...");
        ImGui::MenuItem("Save workspace");
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::BeginDisabled(!session_.draft_editable());
        if (ImGui::MenuItem("Load run plan...")) {
            run_plan_file_action_ = RunPlanFileAction::Load;
            run_plan_file_status_.clear();
            open_run_plan_file_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Save run plan...")) {
            run_plan_file_action_ = RunPlanFileAction::Save;
            run_plan_file_status_.clear();
            open_run_plan_file_dialog_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Experiment Explorer", nullptr, &show_explorer_);
        ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
        ImGui::Separator();
        ImGui::MenuItem("Architecture", nullptr, &show_architecture_);
        ImGui::MenuItem("Scheduling timeline", nullptr, &show_timeline_);
        ImGui::MenuItem("Functional signals", nullptr, &show_signals_);
        ImGui::MenuItem("Resources", nullptr, &show_resources_);
        ImGui::MenuItem("Canonical events", nullptr, &show_events_);
        ImGui::SeparatorText("Text");
        ImGui::TextDisabled("Display scale: %.2fx", ImGui::GetStyle().FontScaleDpi);
        ImGui::SetNextItemWidth(8.0F * ImGui::GetFontSize());
        ImGui::SliderFloat("Text size", &text_scale_, 0.75F, 2.0F, "%.2fx",
                           ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::MenuItem("Reset text size", nullptr, false, text_scale_ != 1.0F)) {
            text_scale_ = 1.0F;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About CPSSim")) {
            open_about_ = true;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

/*** Loads or saves through the shared strict persistence API. ***/
void GuiApplication::draw_run_plan_file_dialog() {
    if (open_run_plan_file_dialog_) {
        ImGui::OpenPopup("Run plan file");
        open_run_plan_file_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("Run plan file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const auto loading = run_plan_file_action_ == RunPlanFileAction::Load;
    ImGui::TextUnformatted(loading ? "Load a validated run plan into the pending draft."
                                   : "Save the currently valid pending draft.");
    ImGui::SetNextItemWidth(34.0F * ImGui::GetFontSize());
    if (ImGui::InputText("Path", run_plan_path_.data(), run_plan_path_.size())) {
        run_plan_file_status_.clear();
    }

    if (!run_plan_file_status_.empty()) {
        const auto color = run_plan_file_error_ ? ImVec4{1.0F, 0.45F, 0.35F, 1.0F}
                                                : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", run_plan_file_status_.c_str());
        ImGui::PopStyleColor();
    }

    if (ImGui::Button(loading ? "Load into draft" : "Save draft")) {
        try {
            const std::filesystem::path path{run_plan_path_.data()};
            if (path.empty()) {
                throw std::invalid_argument{"run plan path: must not be empty"};
            }
            if (loading) {
                const auto plan = load_run_plan(path, session_.config());
                if (!session_.replace_draft(plan)) {
                    throw std::runtime_error{
                        "run plan $: the draft cannot be replaced while the run is Running"};
                }
                run_plan_file_status_ =
                    "Loaded into the pending draft. Use Apply and reset to activate it.";
            } else {
                const auto& validation = session_.validate_draft();
                if (!validation.valid()) {
                    const auto& diagnostic = validation.diagnostics.front();
                    auto location = std::string{"draft"};
                    if (diagnostic.task_id.has_value()) {
                        location = "draft assignment for task T" +
                                   std::to_string(diagnostic.task_id->value());
                    } else if (diagnostic.code == RunPlanDiagnosticCode::InvalidStopTick) {
                        location = "draft stop tick";
                    } else if (diagnostic.code == RunPlanDiagnosticCode::UnsupportedPolicy) {
                        location = "draft scheduling policy";
                    }
                    throw std::invalid_argument{location + ": " + diagnostic.message +
                                                ". See the highlighted Run plan field."};
                }
                save_run_plan(path, session_.config(), *validation.plan);
                run_plan_file_status_ = "Saved run plan to '" + path.string() + "'.";
            }
            run_plan_file_error_ = false;
        } catch (const std::exception& error) {
            run_plan_file_status_ = error.what();
            run_plan_file_error_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

/*** Presents project identity without adding external help behavior. ***/
void GuiApplication::draw_about_dialog() {
    if (open_about_) {
        ImGui::OpenPopup("About CPSSim");
        open_about_ = false;
    }

    if (ImGui::BeginPopupModal("About CPSSim", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto project_version = version();
        ImGui::Text("CPSSim %.*s", static_cast<int>(project_version.size()),
                    project_version.data());
        ImGui::TextUnformatted("Deterministic cyber-physical-system simulation workbench");
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/*** Draws G04/G05 analysis tabs plus the existing runtime/event stack. ***/
void GuiApplication::draw_center_panels(const SimulationSnapshot& snapshot) {
    auto available_height = ImGui::GetContentRegionAvail().y;
    if (show_architecture_ || show_timeline_ || show_signals_) {
        const auto has_lower_panel = show_resources_ || show_events_;
        auto analysis_height = 0.0F;
        if (has_lower_panel) {
            const auto minimum_panel_height = 7.0F * ImGui::GetTextLineHeightWithSpacing();
            const auto maximum_analysis_height =
                std::max(minimum_panel_height, available_height - minimum_panel_height);
            analysis_height =
                std::clamp(available_height * 0.56F, minimum_panel_height, maximum_analysis_height);
        }
        ImGui::BeginChild("Analysis panel", ImVec2{0.0F, analysis_height}, ImGuiChildFlags_Borders);
        if (ImGui::BeginTabBar("Analysis views")) {
            if (show_architecture_ && ImGui::BeginTabItem("Architecture")) {
                const auto graph = build_architecture_graph(snapshot.experiment);
                if (draw_architecture_view(graph, session_, snapshot, selection_,
                                           architecture_view_state_)) {
                    show_inspector_ = true;
                }
                ImGui::EndTabItem();
            }
            if (show_timeline_ && ImGui::BeginTabItem("Scheduling timeline")) {
                if (draw_timeline_view(snapshot, selection_, timeline_view_state_)) {
                    show_inspector_ = true;
                }
                ImGui::EndTabItem();
            }
            if (show_signals_ && ImGui::BeginTabItem("Functional signals")) {
                draw_signal_view(snapshot, selection_, signal_view_state_);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        if (!has_lower_panel) {
            return;
        }
        available_height = ImGui::GetContentRegionAvail().y;
    }

    if (show_resources_ && show_events_) {
        const auto minimum_panel_height = 4.0F * ImGui::GetTextLineHeightWithSpacing();
        const auto minimum_resource_height =
            std::min(minimum_panel_height, available_height * 0.5F);
        const auto maximum_resource_height =
            std::max(minimum_resource_height, available_height - minimum_panel_height);
        const auto resource_height =
            std::clamp(available_height * 0.42F, minimum_resource_height, maximum_resource_height);
        ImGui::BeginChild("Resource panel", ImVec2{0.0F, resource_height}, ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Resources");
        ImGui::Separator();
        draw_resource_view(snapshot, selection_);
        ImGui::EndChild();

        ImGui::BeginChild("Event panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Canonical events");
        ImGui::Separator();
        draw_event_view(snapshot, selection_);
        ImGui::EndChild();
    } else if (show_resources_) {
        ImGui::BeginChild("Resource panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Resources");
        ImGui::Separator();
        draw_resource_view(snapshot, selection_);
        ImGui::EndChild();
    } else if (show_events_) {
        ImGui::BeginChild("Event panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("Canonical events");
        ImGui::Separator();
        draw_event_view(snapshot, selection_);
        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("No panels are visible. Use the View menu to restore a panel.");
    }
}

/*** Fills the native window with toolbar and shared-selection workbench. ***/
void GuiApplication::draw_frame(const SimulationSnapshot& snapshot) {
    ImGui::GetStyle().FontScaleMain = text_scale_;
    synchronize_selection(selection_, snapshot);

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr auto workbench_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    ImGui::Begin("CPSSim Workbench", nullptr, workbench_flags);

    draw_main_menu();

    const auto toolbar_height = 2.0F * ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Simulation toolbar", ImVec2{0.0F, toolbar_height}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    draw_toolbar(session_, snapshot);
    ImGui::EndChild();

    const auto column_count =
        1 + static_cast<int>(show_explorer_) + static_cast<int>(show_inspector_);
    const auto layout_identity =
        static_cast<int>(show_explorer_) | (static_cast<int>(show_inspector_) << 1);
    const auto available_width = ImGui::GetContentRegionAvail().x;
    const auto explorer_width = std::min(18.0F * ImGui::GetFontSize(), available_width * 0.24F);
    const auto inspector_width = std::min(28.0F * ImGui::GetFontSize(), available_width * 0.34F);
    ImGui::PushID(layout_identity);
    if (ImGui::BeginTable("Workbench layout", column_count,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp,
                          ImVec2{0.0F, 0.0F})) {
        if (show_explorer_) {
            ImGui::TableSetupColumn("Explorer", ImGuiTableColumnFlags_WidthFixed, explorer_width);
        }
        ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch);
        if (show_inspector_) {
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, inspector_width);
        }
        ImGui::TableNextRow();

        int column = 0;
        if (show_explorer_) {
            ImGui::TableSetColumnIndex(column++);
            ImGui::BeginChild("Experiment Explorer panel", ImVec2{0.0F, 0.0F},
                              ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted("Experiment Explorer");
            ImGui::Separator();
            draw_experiment_explorer(snapshot.experiment, selection_);
            ImGui::EndChild();
        }

        ImGui::TableSetColumnIndex(column++);
        draw_center_panels(snapshot);

        if (show_inspector_) {
            ImGui::TableSetColumnIndex(column);
            ImGui::BeginChild("Inspector panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
            ImGui::TextUnformatted("Inspector");
            ImGui::Separator();
            draw_run_plan_editor(session_, snapshot);
            ImGui::Separator();
            draw_inspector_view(snapshot, selection_);
            ImGui::EndChild();
        }
        ImGui::EndTable();
    }
    ImGui::PopID();

    ImGui::End();
    draw_about_dialog();
    draw_run_plan_file_dialog();
}

} // namespace cpssim::gui
