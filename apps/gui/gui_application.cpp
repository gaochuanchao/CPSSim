/***
 * File: apps/gui/gui_application.cpp
 * Purpose: Render the fixed, resize-aware CPSSim home and workbench shell.
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

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/project/project_template.hpp"
#include "cpssim/application/project/project_workflow.hpp"
#include "cpssim/config/json_run_plan.hpp"
#include "cpssim/core/version.hpp"

#include "imgui.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace cpssim::gui {

GuiApplication::GuiApplication() {
    paths_.preferences_file = default_gui_preferences_file();
    initialize_presentation_state();
}

GuiApplication::GuiApplication(std::unique_ptr<GuiSimulationSession> session,
                               std::unique_ptr<FileDialog> dialogs, GuiApplicationPaths paths)
    : dialogs_{std::move(dialogs)}, paths_{std::move(paths)} {
    if (session != nullptr) {
        application_state_.replace_session(std::move(session));
    }
    initialize_presentation_state();
}

GuiApplication::GuiApplication(std::unique_ptr<ProjectContext> project,
                               std::unique_ptr<FileDialog> dialogs, GuiApplicationPaths paths)
    : dialogs_{std::move(dialogs)}, paths_{std::move(paths)} {
    if (project != nullptr) {
        application_state_.replace_project(std::move(project));
    }
    initialize_presentation_state();
}

/*** Establishes presentation defaults shared by Home and Workbench startup. ***/
void GuiApplication::initialize_presentation_state() {
    selection_.select_experiment();
    project_parent_ = paths_.projects_directory;
    bosch_parent_ = paths_.projects_directory;
    constexpr std::string_view generic_name{"new-project"};
    std::copy(generic_name.begin(), generic_name.end(), project_name_.begin());
    constexpr std::string_view bosch_name{"bosch-challenge"};
    std::copy(bosch_name.begin(), bosch_name.end(), bosch_project_name_.begin());
    if (!paths_.preferences_file.empty()) {
        auto loaded = load_recent_projects(paths_.preferences_file);
        recent_projects_ = std::move(loaded.recent);
        if (loaded.diagnostic.has_value()) {
            set_status(*loaded.diagnostic, true);
        }
    }
}

ProjectRuntimeResolver GuiApplication::project_runtime_resolver() const {
    const auto reference_root = paths_.bosch_reference_directory;
    const auto shared_library = paths_.bosch_fmu_library;
    return [reference_root, shared_library](const auto& root, const auto& metadata) {
        return resolve_bosch_project_runtime(root, metadata, reference_root, shared_library);
    };
}

void GuiApplication::set_status(std::string message, bool error) {
    application_status_ = std::move(message);
    application_status_error_ = error;
}

void GuiApplication::persist_recents() {
    if (paths_.preferences_file.empty()) {
        return;
    }
    try {
        save_recent_projects(paths_.preferences_file, recent_projects_);
    } catch (const std::exception& error) {
        set_status(error.what(), true);
    }
}

void GuiApplication::record_active_project() {
    if (!application_state_.has_active_project()) {
        return;
    }
    recent_projects_.add(application_state_.active_project().root() / "project.json");
    persist_recents();
    if (application_state_.active_project().workspace_diagnostic().has_value()) {
        set_status(*application_state_.active_project().workspace_diagnostic(), true);
    }
}

/*** Replaces the active session only after its caller has constructed it. ***/
void GuiApplication::replace_session(std::unique_ptr<GuiSimulationSession> session) {
    application_state_.replace_session(std::move(session));
    selection_.select_experiment();
    application_status_.clear();
}

/*** Activates one fully constructed project and its uniquely owned session. ***/
void GuiApplication::replace_project(std::unique_ptr<ProjectContext> project) {
    application_state_.replace_project(std::move(project));
    selection_.select_experiment();
    application_status_.clear();
}

/*** Loads completely before replacing the current project or standalone session. ***/
void GuiApplication::load_project_file(const std::filesystem::path& project_file) {
    auto replacement = load_project(project_file, project_runtime_resolver());
    replace_project(std::move(replacement));
    record_active_project();
}

/*** Persists only specifications and presentation metadata for the active project. ***/
void GuiApplication::save_active_project() { save_project(application_state_.active_project()); }

/*** Returns the application to Home without retaining a session reference. ***/
void GuiApplication::clear_session() {
    application_state_.clear_session();
    selection_.select_experiment();
    application_status_.clear();
}

/*** Advances queued simulation commands only when a session is active. ***/
void GuiApplication::update_active_session() {
    if (application_state_.has_active_session()) {
        application_state_.active_session().update();
    }
}

/*** Updates menu-owned visibility and opens the static About dialog. ***/
bool GuiApplication::draw_main_menu() {
    auto& session = application_state_.active_session();
    bool close_requested = false;
    if (!ImGui::BeginMenuBar()) {
        return true;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Create New Project...")) {
            project_dialog_kind_ = ProjectDialogKind::NewGeneric;
            request_project_modal_ = true;
        }
        if (ImGui::MenuItem("Open Existing Project...")) {
            open_project_dialog();
        }
        if (ImGui::MenuItem("Bosch Challenge Example...")) {
            bosch_step_ = BoschWizardStep::Trajectory;
            open_bosch_wizard_ = true;
        }
        ImGui::Separator();
        ImGui::BeginDisabled(!application_state_.has_active_project());
        if (ImGui::MenuItem("Save Project")) {
            try {
                save_active_project();
                set_status("Project saved.", false);
            } catch (const std::exception& error) {
                set_status(error.what(), true);
            }
        }
        if (ImGui::MenuItem("Save Project As...")) {
            project_dialog_kind_ = ProjectDialogKind::SaveAs;
            project_parent_ = paths_.projects_directory;
            std::fill(project_name_.begin(), project_name_.end(), '\0');
            const auto suggested = application_state_.active_project().metadata().name + "-copy";
            std::copy_n(suggested.begin(),
                        std::min(suggested.size(), project_name_.size() - 1),
                        project_name_.begin());
            request_project_modal_ = true;
        }
        if (ImGui::MenuItem("Close Project")) {
            close_requested = true;
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::BeginDisabled(!session.draft_editable());
        if (ImGui::MenuItem("Load run plan...")) {
            load_run_plan_dialog();
        }
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Save run plan...")) {
            save_run_plan_dialog();
        }
        ImGui::EndMenu();
    }
    if (close_requested) {
        ImGui::EndMenuBar();
        clear_session();
        return false;
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
        const auto display_scale =
            std::max(ImGui::GetStyle().FontScaleDpi, ImGui::GetIO().DisplayFramebufferScale.x);
        ImGui::TextDisabled("Display scale: %.2fx", display_scale);
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
    return true;
}

void GuiApplication::open_project_dialog() {
    if (dialogs_ == nullptr) {
        set_status("File dialogs are unavailable.", true);
        return;
    }
    const auto result = open_project_from_dialog(application_state_, *dialogs_,
                                                 paths_.projects_directory,
                                                 project_runtime_resolver());
    if (result.status == ProjectWorkflowStatus::Applied) {
        selection_.select_experiment();
        set_status("Opened project '" +
                       application_state_.active_project().metadata().name + "'.",
                   false);
        record_active_project();
    } else if (result.status == ProjectWorkflowStatus::Failed) {
        set_status(result.diagnostic, true);
    }
}

void GuiApplication::load_run_plan_dialog() {
    if (dialogs_ == nullptr) {
        set_status("File dialogs are unavailable.", true);
        return;
    }
    const auto result = load_run_plan_from_dialog(application_state_.active_session(), *dialogs_,
                                                  paths_.projects_directory);
    if (result.status == ProjectWorkflowStatus::Applied) {
        set_status("Loaded run plan into the pending draft. Apply and reset to activate it.",
                   false);
    } else if (result.status == ProjectWorkflowStatus::Failed) {
        set_status(result.diagnostic, true);
    }
}

void GuiApplication::save_run_plan_dialog() {
    if (dialogs_ == nullptr) {
        set_status("File dialogs are unavailable.", true);
        return;
    }
    auto suggested = paths_.projects_directory / "run-plan.json";
    if (application_state_.has_active_project()) {
        suggested = application_state_.active_project().root() / "run-plan.json";
    }
    const auto result =
        save_run_plan_from_dialog(application_state_.active_session(), *dialogs_, suggested);
    if (result.status == ProjectWorkflowStatus::Applied) {
        set_status("Saved run plan to '" + result.selected_path.string() + "'.", false);
    } else if (result.status == ProjectWorkflowStatus::Failed) {
        set_status(result.diagnostic, true);
    }
}

void GuiApplication::draw_project_dialog() {
    if (request_project_modal_) {
        ImGui::OpenPopup("Project location");
        request_project_modal_ = false;
    }
    if (!ImGui::BeginPopupModal("Project location", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const auto save_as = project_dialog_kind_ == ProjectDialogKind::SaveAs;
    ImGui::TextUnformatted(save_as ? "Save the active project under a new name."
                                   : "Create a minimal valid generic project.");
    ImGui::SetNextItemWidth(24.0F * ImGui::GetFontSize());
    ImGui::InputText("Project name", project_name_.data(), project_name_.size());
    ImGui::TextWrapped("Parent: %s", project_parent_.string().c_str());
    if (ImGui::Button("Choose parent...")) {
        if (dialogs_ == nullptr) {
            set_status("File dialogs are unavailable.", true);
        } else {
            const auto selection = dialogs_->choose_project_parent(project_parent_);
            if (selection.status == FileDialogStatus::Selected) {
                project_parent_ = selection.path;
            } else if (selection.status == FileDialogStatus::Failed) {
                set_status(selection.diagnostic, true);
            }
        }
    }
    ImGui::Separator();
    if (ImGui::Button(save_as ? "Save As" : "Create")) {
        try {
            std::unique_ptr<ProjectContext> replacement;
            if (save_as) {
                replacement = save_project_as(application_state_.active_project(), project_parent_,
                                              project_name_.data(), project_runtime_resolver());
            } else {
                replacement = create_project(
                    make_generic_project_template(project_parent_, project_name_.data()));
            }
            replace_project(std::move(replacement));
            set_status(save_as ? "Project saved under its new name."
                               : "Generic project created.",
                       false);
            record_active_project();
            ImGui::CloseCurrentPopup();
        } catch (const std::exception& error) {
            set_status(error.what(), true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!application_status_.empty() && application_status_error_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0F, 0.45F, 0.35F, 1.0F});
        ImGui::TextWrapped("%s", application_status_.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndPopup();
}

void GuiApplication::draw_bosch_wizard() {
    if (open_bosch_wizard_) {
        ImGui::OpenPopup("Bosch Challenge Project");
        open_bosch_wizard_ = false;
    }
    if (!ImGui::BeginPopupModal("Bosch Challenge Project", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const auto step_number = static_cast<int>(bosch_step_) + 1;
    ImGui::Text("Step %d of 5", step_number);
    ImGui::Separator();

    if (bosch_step_ == BoschWizardStep::Trajectory) {
        ImGui::TextUnformatted("Trajectory");
        ImGui::RadioButton("example_v_10", &bosch_trajectory_, 0);
        ImGui::RadioButton("example_v_12_5", &bosch_trajectory_, 1);
        ImGui::RadioButton("example_v_15", &bosch_trajectory_, 2);
        ImGui::RadioButton("Custom trajectory directory", &bosch_trajectory_, 3);
        if (bosch_trajectory_ == 3) {
            ImGui::TextWrapped("%s", bosch_custom_trajectory_.empty()
                                          ? "No custom directory selected"
                                          : bosch_custom_trajectory_.string().c_str());
            if (ImGui::Button("Choose trajectory...")) {
                if (dialogs_ == nullptr) {
                    set_status("File dialogs are unavailable.", true);
                } else {
                    const auto selection =
                        dialogs_->choose_trajectory_directory(paths_.examples_directory);
                    if (selection.status == FileDialogStatus::Selected) {
                        bosch_custom_trajectory_ = selection.path;
                    } else if (selection.status == FileDialogStatus::Failed) {
                        set_status(selection.diagnostic, true);
                    }
                }
            }
        }
    } else if (bosch_step_ == BoschWizardStep::Scenario) {
        ImGui::TextUnformatted("Scenario");
        ImGui::RadioButton("Dedicated", &bosch_scenario_, 0);
        ImGui::RadioButton("Shared cloud", &bosch_scenario_, 1);
    } else if (bosch_step_ == BoschWizardStep::Horizon) {
        ImGui::TextUnformatted("Horizon");
        if (ImGui::RadioButton("Complete trajectory", bosch_complete_horizon_)) {
            bosch_complete_horizon_ = true;
        }
        if (ImGui::RadioButton("Custom stop tick", !bosch_complete_horizon_)) {
            bosch_complete_horizon_ = false;
        }
        if (!bosch_complete_horizon_) {
            ImGui::InputText("Stop tick", bosch_stop_tick_.data(), bosch_stop_tick_.size(),
                             ImGuiInputTextFlags_CharsDecimal);
        }
    } else if (bosch_step_ == BoschWizardStep::Project) {
        ImGui::TextUnformatted("Project");
        ImGui::InputText("Project name", bosch_project_name_.data(),
                         bosch_project_name_.size());
        ImGui::TextWrapped("Parent: %s", bosch_parent_.string().c_str());
        if (ImGui::Button("Choose parent...")) {
            if (dialogs_ == nullptr) {
                set_status("File dialogs are unavailable.", true);
            } else {
                const auto selection = dialogs_->choose_project_parent(bosch_parent_);
                if (selection.status == FileDialogStatus::Selected) {
                    bosch_parent_ = selection.path;
                } else if (selection.status == FileDialogStatus::Failed) {
                    set_status(selection.diagnostic, true);
                }
            }
        }
    } else {
        static constexpr std::array<std::string_view, 3> trajectories{
            "example_v_10", "example_v_12_5", "example_v_15"};
        const auto trajectory_name = bosch_trajectory_ == 3
                                         ? bosch_custom_trajectory_.string()
                                         : std::string{trajectories[static_cast<std::size_t>(
                                               bosch_trajectory_)]};
        ImGui::TextUnformatted("Review and Create");
        ImGui::Text("Trajectory: %s", trajectory_name.c_str());
        ImGui::Text("Scenario: %s", bosch_scenario_ == 0 ? "dedicated" : "shared_cloud");
        ImGui::Text("Horizon: %s", bosch_complete_horizon_ ? "complete trajectory"
                                                           : bosch_stop_tick_.data());
        ImGui::Text("Project: %s", bosch_project_name_.data());
        ImGui::TextWrapped("Parent: %s", bosch_parent_.string().c_str());
    }

    ImGui::Separator();
    if (bosch_step_ != BoschWizardStep::Trajectory && ImGui::Button("Back")) {
        bosch_step_ = static_cast<BoschWizardStep>(static_cast<int>(bosch_step_) - 1);
    }
    if (bosch_step_ != BoschWizardStep::Trajectory) {
        ImGui::SameLine();
    }
    if (bosch_step_ != BoschWizardStep::Review) {
        if (ImGui::Button("Next")) {
            if (bosch_step_ == BoschWizardStep::Trajectory && bosch_trajectory_ == 3 &&
                bosch_custom_trajectory_.empty()) {
                set_status("Choose a custom trajectory directory before continuing.", true);
            } else {
                bosch_step_ = static_cast<BoschWizardStep>(static_cast<int>(bosch_step_) + 1);
            }
        }
    } else if (ImGui::Button("Create")) {
        try {
            static constexpr std::array<std::string_view, 3> trajectories{
                "example_v_10", "example_v_12_5", "example_v_15"};
            const auto trajectory = bosch_trajectory_ == 3
                                        ? bosch_custom_trajectory_
                                        : paths_.examples_directory /
                                              trajectories[static_cast<std::size_t>(
                                                  bosch_trajectory_)];
            std::optional<Tick> stop_tick;
            if (!bosch_complete_horizon_) {
                const std::string_view text{bosch_stop_tick_.data()};
                std::uint64_t parsed = 0;
                const auto parsed_result =
                    std::from_chars(text.data(), text.data() + text.size(), parsed);
                if (text.empty() || parsed_result.ec != std::errc{} ||
                    parsed_result.ptr != text.data() + text.size() ||
                    parsed > static_cast<std::uint64_t>(std::numeric_limits<Tick>::max())) {
                    throw std::invalid_argument{"stop tick must be a nonnegative integer Tick"};
                }
                stop_tick = static_cast<Tick>(parsed);
            }
            auto replacement = create_bosch_project(
                {.parent_directory = bosch_parent_,
                 .name = bosch_project_name_.data(),
                 .trajectory_directory = trajectory,
                 .scenario = bosch_scenario_ == 0 ? BoschReferenceScenario::Dedicated
                                                  : BoschReferenceScenario::SharedCloud,
                 .stop_tick = stop_tick,
                 .reference_root = paths_.bosch_reference_directory,
                 .shared_library = paths_.bosch_fmu_library});
            replace_project(std::move(replacement));
            set_status("Bosch Challenge project created paused.", false);
            record_active_project();
            ImGui::CloseCurrentPopup();
        } catch (const std::exception& error) {
            set_status(error.what(), true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!application_status_.empty() && application_status_error_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0F, 0.45F, 0.35F, 1.0F});
        ImGui::TextWrapped("%s", application_status_.c_str());
        ImGui::PopStyleColor();
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
    auto& session = application_state_.active_session();
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
                if (draw_architecture_view(graph, session, snapshot, selection_,
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

/*** Draws the existing shared-selection workbench for an active session. ***/
void GuiApplication::draw_workbench(const SimulationSnapshot& snapshot) {
    auto& session = application_state_.active_session();
    synchronize_selection(selection_, snapshot);
    if (!draw_main_menu()) {
        return;
    }

    const auto toolbar_height = 2.0F * ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Simulation toolbar", ImVec2{0.0F, toolbar_height}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    draw_toolbar(session, snapshot);
    ImGui::EndChild();

    if (!application_status_.empty()) {
        const auto color = application_status_error_ ? ImVec4{1.0F, 0.45F, 0.35F, 1.0F}
                                                     : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", application_status_.c_str());
        ImGui::PopStyleColor();
    }

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
            draw_run_plan_editor(session, snapshot);
            ImGui::Separator();
            draw_inspector_view(snapshot, selection_);
            ImGui::EndChild();
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

/*** Draws functional Goal 1 startup choices without constructing a dummy session. ***/
void GuiApplication::draw_home_screen() {
    const auto available = ImGui::GetContentRegionAvail();
    constexpr float button_width = 20.0F;
    const auto scaled_button_width = button_width * ImGui::GetFontSize();
    const auto content_left = ImGui::GetCursorPosX();
    const auto left = content_left + std::max(0.0F, (available.x - scaled_button_width) * 0.5F);
    const auto top = std::max(0.0F, available.y * 0.18F);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + top);

    const auto title = std::string_view{"CPSSim"};
    const auto title_width = ImGui::CalcTextSize(title.data(), title.data() + title.size()).x;
    ImGui::SetCursorPosX(content_left + std::max(0.0F, (available.x - title_width) * 0.5F));
    ImGui::TextUnformatted(title.data(), title.data() + title.size());

    const auto subtitle = std::string_view{"Start a simulation project"};
    const auto subtitle_width =
        ImGui::CalcTextSize(subtitle.data(), subtitle.data() + subtitle.size()).x;
    ImGui::SetCursorPosX(content_left + std::max(0.0F, (available.x - subtitle_width) * 0.5F));
    ImGui::TextDisabled("%.*s", static_cast<int>(subtitle.size()), subtitle.data());
    ImGui::Spacing();

    ImGui::SetCursorPosX(left);
    if (ImGui::Button("Create New Project", ImVec2{scaled_button_width, 0.0F})) {
        project_dialog_kind_ = ProjectDialogKind::NewGeneric;
        project_parent_ = paths_.projects_directory;
        request_project_modal_ = true;
    }
    ImGui::SetCursorPosX(left);
    if (ImGui::Button("Open Existing Project", ImVec2{scaled_button_width, 0.0F})) {
        open_project_dialog();
    }
    ImGui::SetCursorPosX(left);
    if (ImGui::Button("Bosch Challenge Example", ImVec2{scaled_button_width, 0.0F})) {
        bosch_step_ = BoschWizardStep::Trajectory;
        open_bosch_wizard_ = true;
    }

    recent_projects_.refresh_availability();
    if (!recent_projects_.entries().empty()) {
        ImGui::Spacing();
        ImGui::SetCursorPosX(left);
        ImGui::TextUnformatted("Recent projects");
        for (const auto& entry : recent_projects_.entries()) {
            ImGui::PushID(entry.project_file.string().c_str());
            ImGui::SetCursorPosX(left);
            ImGui::BeginDisabled(!entry.available);
            if (ImGui::Button(entry.project_file.parent_path().filename().string().c_str(),
                              ImVec2{scaled_button_width * 0.78F, 0.0F})) {
                try {
                    auto replacement = load_project(entry.project_file, project_runtime_resolver());
                    replace_project(std::move(replacement));
                    record_active_project();
                } catch (const std::exception& error) {
                    set_status(error.what(), true);
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                const auto removed_path = entry.project_file;
                recent_projects_.remove(removed_path);
                persist_recents();
                ImGui::PopID();
                break;
            }
            if (!entry.available && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Project is unavailable");
            }
            ImGui::PopID();
        }
    }

    if (!application_status_.empty()) {
        ImGui::Spacing();
        ImGui::SetCursorPosX(left);
        ImGui::PushTextWrapPos(left + scaled_button_width);
        const auto color = application_status_error_ ? ImVec4{1.0F, 0.45F, 0.35F, 1.0F}
                                                     : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", application_status_.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }
}

/*** Fills the native window with Home or a detached-snapshot workbench. ***/
void GuiApplication::draw_frame() {
    ImGui::GetStyle().FontScaleMain = text_scale_;

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    auto window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_NoNavFocus;
    const auto workbench_active = application_state_.screen() == GuiApplicationScreen::Workbench;
    if (workbench_active) {
        window_flags |= ImGuiWindowFlags_MenuBar;
    }
    ImGui::Begin("CPSSim", nullptr, window_flags);

    if (!workbench_active) {
        draw_home_screen();
    } else {
        const auto snapshot = application_state_.active_session().snapshot();
        draw_workbench(snapshot);
    }

    ImGui::End();
    if (application_state_.has_active_session()) {
        draw_about_dialog();
    }
    draw_project_dialog();
    draw_bosch_wizard();
}

} // namespace cpssim::gui
