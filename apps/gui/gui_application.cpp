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
#include "views/plot_visualizer.hpp"
#include "views/resource_view.hpp"
#include "views/results_view.hpp"
#include "views/run_plan_editor.hpp"
#include "views/signal_view.hpp"
#include "views/system_builder.hpp"
#include "views/timeline_view.hpp"
#include "views/toolbar_view.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/bosch_result_analysis.hpp"
#include "cpssim/application/project/project_template.hpp"
#include "cpssim/application/project/project_workflow.hpp"
#include "cpssim/application/project/system_builder_workflow.hpp"
#include "cpssim/application/result_export.hpp"
#include "cpssim/config/json_run_plan.hpp"
#include "cpssim/core/version.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace cpssim::gui {
namespace {

float sidebar_top_height(float available_height, float splitter_height, float ratio) {
    const auto usable = std::max(0.0F, available_height - splitter_height);
    const auto desired_minimum = 6.0F * ImGui::GetTextLineHeightWithSpacing();
    const auto minimum = std::min(desired_minimum, usable * 0.45F);
    return std::clamp(usable * ratio, minimum, std::max(minimum, usable - minimum));
}

float horizontal_splitter_height() { return std::max(2.0F, ImGui::GetFrameHeight() * 0.12F); }

void remove_vertical_item_spacing() {
    ImGui::SetCursorPosY(std::max(0.0F, ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y));
}

void draw_horizontal_splitter(const char* identity, float available_height, float splitter_height,
                              float& ratio, GuiPointerRegionMap* pointer_regions = nullptr) {
    const auto usable = std::max(0.0F, available_height - splitter_height);
    const auto desired_minimum = 6.0F * ImGui::GetTextLineHeightWithSpacing();
    const auto minimum = std::min(desired_minimum, usable * 0.45F);
    remove_vertical_item_spacing();
    ImGui::InvisibleButton(identity, ImVec2{-1.0F, splitter_height});
    if (pointer_regions != nullptr) {
        const auto item_minimum = ImGui::GetItemRectMin();
        const auto item_maximum = ImGui::GetItemRectMax();
        pointer_regions->add(
            {ImGui::GetID(identity),
             {item_minimum.x, item_minimum.y, item_maximum.x, item_maximum.y},
             GuiPointerRegionBehavior::DragHandle});
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive() && usable > 0.0F) {
        const auto current = sidebar_top_height(available_height, splitter_height, ratio);
        ratio = std::clamp((current + ImGui::GetIO().MouseDelta.y) / usable, minimum / usable,
                           (usable - minimum) / usable);
    }
    remove_vertical_item_spacing();
}

SystemRunConfigurationDraft
system_run_configuration(const GuiSimulationSession& session,
                         const std::vector<DraftTaskAssignment>& assignments) {
    return {.stop_tick = session.draft().stop_tick(),
            .policy_kind = session.draft().policy_kind(),
            .assignments = assignments};
}

std::string default_export_run_id() {
    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream result;
    result << "run-" << std::put_time(&utc, "%Y%m%d-%H%M%S");
    return result.str();
}

} // namespace

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
    if (completed_finalizer_ == nullptr) {
        completed_finalizer_ = std::make_unique<CompletedRunFinalizer>(
            [](const CompletedRunFinalizationRequest& request, std::stop_token stop) {
                const auto started = std::chrono::steady_clock::now();
                if (stop.stop_requested()) {
                    return CompletedRunResult{};
                }
                auto result = std::make_shared<const RunResult>(
                    build_run_result(request.data, request.scenario_kind));
                std::shared_ptr<const BoschResultAnalysis> bosch_analysis;
                if (!stop.stop_requested() && request.scenario_kind == "bosch") {
                    bosch_analysis = std::make_shared<const BoschResultAnalysis>(
                        derive_bosch_result_analysis(*result));
                }
                return CompletedRunResult{
                    request.data->runtime_generation, std::move(result),
                    std::move(bosch_analysis), request.performance,
                    std::chrono::steady_clock::now() - started};
            });
    }
    structural_selection_.select_system();
    runtime_selection_.clear();
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
    load_workspace_state();
    initialize_system_draft();
    initialize_imgui_layout();
    if (application_state_.has_active_session()) {
        publish_complete_snapshot(false);
        progress_ = application_state_.active_session().progress();
    }
    last_run_mode_ = workspace_state_.run_mode;
}

void GuiApplication::set_background_wakeup(std::function<void()> wakeup) {
    completed_finalizer_->set_wakeup(std::move(wakeup));
}

bool GuiApplication::process_background_publications() {
    if (completed_finalizer_ == nullptr || !completed_finalizer_->publication_pending()) {
        return false;
    }
    if (auto completed = completed_finalizer_->take_publication(); completed.has_value()) {
        profiler_.record(GuiProfileTimer::ResultFinalization,
                         completed->finalization_duration);
        profiler_.increment(GuiProfileCounter::ResultBuild);
        if (completed->bosch_analysis != nullptr) {
            profiler_.increment(GuiProfileCounter::BoschAnalysisBuild);
        }
        static_cast<void>(completed_results_.publish_ready(std::move(*completed)));
        return true;
    }
    if (const auto diagnostic = completed_finalizer_->diagnostic(); diagnostic.has_value()) {
        set_status("Completed-run finalization failed: " + *diagnostic, true);
        return true;
    }
    return false;
}

void GuiApplication::shutdown_background_work() {
    if (completed_finalizer_ != nullptr) {
        completed_finalizer_->set_wakeup({});
        completed_finalizer_->cancel();
    }
}

void GuiApplication::invalidate_completed_results() {
    if (completed_finalizer_ != nullptr) {
        completed_finalizer_->reset();
    }
    completed_results_.invalidate();
}

void GuiApplication::initialize_imgui_layout() {
    if (paths_.default_imgui_layout.empty() || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    imgui_layout_store_ = std::make_unique<GuiLayoutStore>(paths_.default_imgui_layout);
    activate_imgui_layout();
}

void GuiApplication::activate_imgui_layout() {
    if (imgui_layout_store_ == nullptr) {
        return;
    }
    const auto project_root = application_state_.has_active_project()
                                  ? std::optional{application_state_.active_project().root()}
                                  : std::nullopt;
    auto activation = imgui_layout_store_->activate(project_root);
    pending_imgui_layout_ = std::move(activation.settings);
    if (activation.diagnostic.has_value()) {
        set_status(*activation.diagnostic, true);
    }
}

void GuiApplication::apply_pending_imgui_layout() {
    if (!pending_imgui_layout_.has_value()) {
        return;
    }
    ImGui::ClearIniSettings();
    ImGui::LoadIniSettingsFromMemory(pending_imgui_layout_->data(), pending_imgui_layout_->size());
    ImGui::GetIO().WantSaveIniSettings = false;
    pending_imgui_layout_.reset();
}

void GuiApplication::capture_current_imgui_layout() {
    if (imgui_layout_store_ == nullptr || pending_imgui_layout_.has_value()) {
        return;
    }
    std::size_t size = 0;
    const auto* settings = ImGui::SaveIniSettingsToMemory(&size);
    imgui_layout_store_->record_current(std::string{settings, size});
    ImGui::GetIO().WantSaveIniSettings = false;
}

void GuiApplication::save_active_imgui_layout() {
    if (imgui_layout_store_ == nullptr || !application_state_.has_active_project()) {
        return;
    }
    capture_current_imgui_layout();
    imgui_layout_store_->save_to_project(application_state_.active_project().root());
}

void GuiApplication::restore_default_imgui_layout() {
    if (imgui_layout_store_ == nullptr) {
        set_status("The default GUI layout is unavailable.", true);
        return;
    }
    pending_imgui_layout_ = imgui_layout_store_->restore_default();
    set_status(application_state_.has_active_project()
                   ? "Default layout restored. Save Project to keep it for this project."
                   : "Default layout restored.",
               false);
}

void GuiApplication::load_workspace_state() {
    workspace_state_ = application_state_.has_active_project()
                           ? application_state_.active_project().workspace()
                           : GuiWorkspaceState{};
    normalize_workspace_state(workspace_state_);
    signal_view_state_.selected_signals = workspace_state_.selected_signals;
    signal_view_state_.selection_initialized = !workspace_state_.selected_signals.empty();
    event_view_state_.filter_initialized = false;
    results_view_state_ = {};
    plot_visualizer_view_state_ = {};
    open_plot_visualizer_ = false;
    restore_center_tabs_ = true;
}

void GuiApplication::synchronize_project_workspace() {
    workspace_state_.selected_signals = signal_view_state_.selected_signals;
    normalize_workspace_state(workspace_state_);
    if (application_state_.has_active_project()) {
        application_state_.active_project().set_workspace(workspace_state_);
    }
}

void GuiApplication::initialize_system_draft() {
    system_draft_.reset();
    system_validation_ = {};
    system_run_assignments_.clear();
    system_builder_view_state_ = {};
    explorer_view_state_ = {};
    if (application_state_.has_active_project() &&
        project_system_edit_policy(application_state_.active_project().metadata()) !=
            ProjectSystemEditPolicy::ReadOnlyAdapter) {
        system_draft_.emplace(application_state_.active_session().config());
        system_validation_ = system_draft_->build();
        if (const auto* plan = application_state_.active_session().active_plan(); plan != nullptr) {
            system_run_assignments_.reserve(plan->assignments().size());
            for (const auto& assignment : plan->assignments()) {
                system_run_assignments_.push_back(
                    {.task_id = assignment.task_id, .resource_id = assignment.resource_id});
            }
        }
    }
}

bool GuiApplication::system_changes_dirty() const {
    if (!system_draft_.has_value()) {
        return false;
    }
    if (system_draft_->dirty()) {
        return true;
    }
    if (application_state_.active_session().draft_dirty()) {
        return true;
    }
    const auto* active = application_state_.active_session().active_plan();
    if (active == nullptr || active->assignments().size() != system_run_assignments_.size()) {
        return true;
    }
    for (const auto& assignment : system_run_assignments_) {
        const auto found = std::find_if(active->assignments().begin(), active->assignments().end(),
                                        [&assignment](const auto& candidate) {
                                            return candidate.task_id == assignment.task_id;
                                        });
        if (found == active->assignments().end() || !assignment.resource_id.has_value() ||
            found->resource_id != *assignment.resource_id) {
            return true;
        }
    }
    return false;
}

void GuiApplication::synchronize_system_assignments() {
    if (!system_draft_.has_value()) {
        return;
    }
    std::vector<DraftTaskAssignment> synchronized;
    synchronized.reserve(system_draft_->tasks().size());
    for (const auto& task : system_draft_->tasks()) {
        const auto found =
            std::find_if(system_run_assignments_.begin(), system_run_assignments_.end(),
                         [&task](const auto& row) { return row.task_id == task.id; });
        synchronized.push_back({.task_id = task.id,
                                .resource_id = found == system_run_assignments_.end()
                                                   ? std::nullopt
                                                   : found->resource_id});
    }
    system_run_assignments_ = std::move(synchronized);
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
    invalidate_completed_results();
    application_state_.replace_session(std::move(session));
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    application_status_.clear();
    activate_imgui_layout();
    publish_complete_snapshot(false);
    progress_ = application_state_.active_session().progress();
    last_run_mode_ = workspace_state_.run_mode;
}

/*** Activates one fully constructed project and its uniquely owned session. ***/
void GuiApplication::replace_project(std::unique_ptr<ProjectContext> project) {
    invalidate_completed_results();
    application_state_.replace_project(std::move(project));
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    application_status_.clear();
    activate_imgui_layout();
    publish_complete_snapshot(false);
    progress_ = application_state_.active_session().progress();
    last_run_mode_ = workspace_state_.run_mode;
}

/*** Loads completely before replacing the current project or standalone session. ***/
void GuiApplication::load_project_file(const std::filesystem::path& project_file) {
    auto replacement = load_project(project_file, project_runtime_resolver());
    replace_project(std::move(replacement));
    record_active_project();
}

/*** Persists only specifications and presentation metadata for the active project. ***/
void GuiApplication::save_active_project() {
    synchronize_project_workspace();
    save_project(application_state_.active_project());
    save_active_imgui_layout();
}

/*** Returns the application to Home without retaining a session reference. ***/
void GuiApplication::clear_session() {
    invalidate_completed_results();
    application_state_.clear_session();
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    application_status_.clear();
    activate_imgui_layout();
    presentation_snapshot_.reset();
    progress_ = {};
}

void GuiApplication::publish_complete_snapshot(bool publish_finished_result) {
    auto& session = application_state_.active_session();
    GuiScopedProfileTimer timer{profiler_, GuiProfileTimer::SnapshotBuild};
    auto snapshot = session.snapshot();
    profiler_.increment(GuiProfileCounter::SnapshotBuild);
    if (publish_finished_result && snapshot.run_state == GuiRunState::Finished) {
        auto data = std::make_shared<const CompletedRunData>(CompletedRunData{
            session.runtime_generation(), session.simulation_data_generation(),
            std::move(snapshot)});
        presentation_snapshot_ =
            std::shared_ptr<const SimulationSnapshot>{data, &data->snapshot};
        const auto scenario = application_state_.has_active_project()
                                  ? application_state_.active_project().metadata().scenario_kind
                                  : std::string{"generic"};
        static_cast<void>(completed_finalizer_->request(
            {std::move(data), scenario, session.performance_summary()}));
    } else {
        presentation_snapshot_ = std::make_shared<const SimulationSnapshot>(std::move(snapshot));
    }
    publication_policy_.published(
        {.mode = workspace_state_.run_mode,
         .update = {},
         .switched_fast_to_live = false,
         .missing_snapshot = false,
         .runtime_generation = session.runtime_generation(),
         .simulation_data_generation = session.simulation_data_generation(),
         .now = std::chrono::steady_clock::now()});
}

/*** Advances queued simulation commands only when a session is active. ***/
bool GuiApplication::update_active_session() {
    if (application_state_.has_active_session() &&
        application_state_.active_session().needs_update()) {
        const auto previous = progress_;
        auto& session = application_state_.active_session();
        const auto settings =
            GuiExecutionSettings{.mode = workspace_state_.run_mode,
                                 .batch_unit = workspace_state_.fast_batch_unit,
                                 .event_batch_size = workspace_state_.fast_event_batch_size,
                                 .tick_batch_size = workspace_state_.fast_tick_batch_size};
        const auto update = session.update(settings);
        progress_ = session.progress();
        const auto switched_to_live =
            last_run_mode_ == GuiRunMode::Fast && workspace_state_.run_mode == GuiRunMode::Live;
        const auto publication = GuiPresentationPublicationInput{
            .mode = workspace_state_.run_mode,
            .update = update,
            .switched_fast_to_live = switched_to_live,
            .missing_snapshot = presentation_snapshot_ == nullptr,
            .runtime_generation = session.runtime_generation(),
            .simulation_data_generation = session.simulation_data_generation(),
            .now = std::chrono::steady_clock::now()};
        if (publication_policy_.should_publish(publication)) {
            publish_complete_snapshot(update.finished);
        }
        if (update.reset) {
            invalidate_completed_results();
        }
        last_run_mode_ = workspace_state_.run_mode;
        return update.reset || update.paused || update.finished || update.transitions != 0 ||
               progress_.run_state != previous.run_state ||
               progress_.current_tick != previous.current_tick ||
               progress_.event_count != previous.event_count;
    }
    return false;
}

void GuiApplication::update_imgui_layout_persistence() {
    if (ImGui::GetIO().WantSaveIniSettings) {
        capture_current_imgui_layout();
    }
}

/*** Updates menu-owned visibility and opens the static About dialog. ***/
bool GuiApplication::draw_main_menu() {
    auto& session = application_state_.active_session();
    if (!ImGui::BeginMenuBar()) {
        return true;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Create New Project...")) {
            request_project_action(PendingProjectAction::CreateGeneric);
        }
        if (ImGui::MenuItem("Open Existing Project...")) {
            request_project_action(PendingProjectAction::OpenDialog);
        }
        if (ImGui::MenuItem("Bosch Challenge Example...")) {
            request_project_action(PendingProjectAction::BoschWizard);
        }
        ImGui::Separator();
        ImGui::BeginDisabled(!application_state_.has_active_project());
        if (ImGui::MenuItem("Save Project")) {
            try {
                save_active_project();
                set_status(system_changes_dirty()
                               ? "Applied project saved; unapplied system changes were not saved."
                               : "Project saved.",
                           false);
            } catch (const std::exception& error) {
                set_status(error.what(), true);
            }
        }
        if (ImGui::MenuItem("Save Project As...")) {
            request_project_action(PendingProjectAction::SaveAs);
        }
        ImGui::BeginDisabled(completed_results_.get() == nullptr);
        if (ImGui::MenuItem("Export Completed Results...")) {
            const auto run_id = default_export_run_id();
            export_run_id_.fill('\0');
            std::copy_n(run_id.begin(), std::min(run_id.size(), export_run_id_.size() - 1),
                        export_run_id_.begin());
            export_destination_ = application_state_.active_project().root() / "results";
            export_scope_ = 0;
            export_diagnostic_.clear();
            open_export_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Close Project")) {
            request_project_action(PendingProjectAction::Close);
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
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Experiment Explorer", nullptr, &workspace_state_.panels.explorer);
        ImGui::MenuItem("System Builder", nullptr, &workspace_state_.panels.system_builder);
        ImGui::MenuItem("Run / Runtime sidebar", nullptr, &workspace_state_.panels.inspector);
        ImGui::Separator();
        ImGui::MenuItem("Architecture", nullptr, &workspace_state_.panels.architecture);
        ImGui::MenuItem("Scheduling timeline", nullptr, &workspace_state_.panels.timeline);
        ImGui::MenuItem("Functional signals", nullptr, &workspace_state_.panels.signals);
        ImGui::MenuItem("Results", nullptr, &workspace_state_.panels.results);
        ImGui::MenuItem("Resources", nullptr, &workspace_state_.panels.resources);
        ImGui::MenuItem("Canonical events", nullptr, &workspace_state_.panels.events);
        if (ImGui::BeginMenu("Theme")) {
            if (ImGui::MenuItem("Light", nullptr, workspace_state_.theme == GuiTheme::Light)) {
                workspace_state_.theme = GuiTheme::Light;
            }
            if (ImGui::MenuItem("Dark", nullptr, workspace_state_.theme == GuiTheme::Dark)) {
                workspace_state_.theme = GuiTheme::Dark;
            }
            ImGui::EndMenu();
        }
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
        ImGui::Separator();
        if (ImGui::MenuItem("Restore Default Layout")) {
            restore_default_imgui_layout();
        }
        if (ImGui::MenuItem("Reset Panel Arrangement")) {
            reset_center_tab_arrangement(workspace_state_);
            restore_center_tabs_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About CPSSim")) {
            open_about_ = true;
        }
        ImGui::EndMenu();
    }

#if !defined(NDEBUG)
    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("GUI Profiler", nullptr, &open_profiler_);
        ImGui::EndMenu();
    }
#endif

    ImGui::EndMenuBar();
    if (pending_project_action_ != PendingProjectAction::None) {
        if (system_changes_dirty()) {
            request_unapplied_modal_ = true;
            return true;
        }
        return !execute_pending_project_action();
    }
    return true;
}

void GuiApplication::draw_export_dialog() {
    if (open_export_dialog_) {
        ImGui::OpenPopup("Export Run Results");
        open_export_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("Export Run Results", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!application_state_.has_active_project() || completed_results_.get() == nullptr) {
        ImGui::TextDisabled("Finish a project run before exporting completed results.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }
    ImGui::InputText("Run ID", export_run_id_.data(), export_run_id_.size());
    ImGui::TextUnformatted("Destination");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", export_destination_.string().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        if (dialogs_ == nullptr) {
            export_diagnostic_ = "File dialogs are unavailable.";
        } else {
            const auto choice = dialogs_->choose_results_directory(export_destination_);
            if (choice.status == FileDialogStatus::Selected) {
                export_destination_ = choice.path;
                export_diagnostic_.clear();
            } else if (choice.status == FileDialogStatus::Failed) {
                export_diagnostic_ = choice.diagnostic;
            }
        }
    }
    const auto range = runtime_selection_.tick_range();
    ImGui::RadioButton("Complete run", &export_scope_, 0);
    ImGui::BeginDisabled(!range.has_value());
    ImGui::RadioButton("Selected time range", &export_scope_, 1);
    ImGui::EndDisabled();
    if (export_scope_ == 1 && range.has_value()) {
        ImGui::SameLine();
        ImGui::TextDisabled("[%lld, %lld]", static_cast<long long>(range->begin_tick),
                            static_cast<long long>(range->end_tick));
    }
    auto raw_enabled = true;
    ImGui::BeginDisabled();
    ImGui::Checkbox("Raw JSON/CSV (authoritative)", &raw_enabled);
    ImGui::EndDisabled();
    ImGui::Checkbox("Excel workbook", &export_excel_);
    if (!export_diagnostic_.empty()) {
        ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.35F, 1.0F}, "%s", export_diagnostic_.c_str());
    }
    if (ImGui::Button("Export")) {
        try {
            const auto& project = application_state_.active_project();
            const auto& result = *completed_results_.get()->result;
            RunScenarioMetadata scenario;
            std::vector<WorkbookControlMetric> control_metrics;
            if (project.metadata().scenario_kind == "bosch") {
                scenario.bosch_trajectory = "trajectory";
                scenario.fmu_identity = "LateralMotionControl";
                scenario.fmu_path = paths_.bosch_fmu_library;
                control_metrics = bosch_workbook_control_metrics(result);
            }
            const RunExportOptions options{
                .destination_directory = export_destination_,
                .run_id = export_run_id_.data(),
                .scope =
                    export_scope_ == 1 ? RunExportScope::SelectedRange : RunExportScope::Complete,
                .selected_range = export_scope_ == 1 ? range : std::nullopt,
                .include_excel = export_excel_,
                .scenario = std::move(scenario),
                .control_metrics = std::move(control_metrics),
                .created_at_utc = {}};
            const auto exported = export_run_result(project, result, options);
            set_status("Run results exported to " + exported.run_directory.string(), false);
            ImGui::CloseCurrentPopup();
        } catch (const std::exception& error) {
            export_diagnostic_ = error.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        export_diagnostic_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApplication::open_project_dialog() {
    if (dialogs_ == nullptr) {
        set_status("File dialogs are unavailable.", true);
        return;
    }
    const auto result = open_project_from_dialog(
        application_state_, *dialogs_, paths_.projects_directory, project_runtime_resolver());
    if (result.status == ProjectWorkflowStatus::Applied) {
        invalidate_completed_results();
        load_workspace_state();
        initialize_system_draft();
        structural_selection_.select_system();
        runtime_selection_.clear();
        publish_complete_snapshot(false);
        progress_ = application_state_.active_session().progress();
        last_run_mode_ = workspace_state_.run_mode;
        set_status("Opened project '" + application_state_.active_project().metadata().name + "'.",
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
        set_status("Loaded run plan into the pending draft. Apply and restart to activate it.",
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

void GuiApplication::request_project_action(PendingProjectAction action,
                                            std::filesystem::path project_file) {
    pending_project_action_ = action;
    pending_project_file_ = std::move(project_file);
}

bool GuiApplication::execute_pending_project_action() {
    const auto action = pending_project_action_;
    const auto project_file = pending_project_file_;
    pending_project_action_ = PendingProjectAction::None;
    pending_project_file_.clear();
    switch (action) {
    case PendingProjectAction::None:
        return false;
    case PendingProjectAction::CreateGeneric:
        project_dialog_kind_ = ProjectDialogKind::NewGeneric;
        project_parent_ = paths_.projects_directory;
        request_project_modal_ = true;
        return false;
    case PendingProjectAction::OpenDialog:
        open_project_dialog();
        return true;
    case PendingProjectAction::OpenRecent:
        try {
            load_project_file(project_file);
        } catch (const std::exception& error) {
            set_status(error.what(), true);
        }
        return true;
    case PendingProjectAction::BoschWizard:
        bosch_step_ = BoschWizardStep::Trajectory;
        open_bosch_wizard_ = true;
        return false;
    case PendingProjectAction::SaveAs: {
        project_dialog_kind_ = ProjectDialogKind::SaveAs;
        project_parent_ = paths_.projects_directory;
        std::fill(project_name_.begin(), project_name_.end(), '\0');
        const auto suggested = application_state_.active_project().metadata().name + "-copy";
        std::copy_n(suggested.begin(), std::min(suggested.size(), project_name_.size() - 1),
                    project_name_.begin());
        request_project_modal_ = true;
        return false;
    }
    case PendingProjectAction::Close:
        clear_session();
        return true;
    }
    return false;
}

void GuiApplication::draw_unapplied_changes_dialog() {
    if (request_unapplied_modal_) {
        ImGui::OpenPopup("Unapplied system changes");
        request_unapplied_modal_ = false;
    }
    if (!ImGui::BeginPopupModal("Unapplied system changes", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("The System Builder has unapplied changes. Choose how to continue.");
    if (ImGui::Button("Apply and save")) {
        synchronize_project_workspace();
        const auto run_configuration =
            system_run_configuration(application_state_.active_session(), system_run_assignments_);
        const auto result = resolve_unapplied_system_changes(
            application_state_, system_draft_ ? &*system_draft_ : nullptr,
            UnappliedSystemDecision::ApplyAndSave, &run_configuration);
        if (result.status == ProjectTransitionStatus::Proceed) {
            try {
                save_active_imgui_layout();
                initialize_system_draft();
                ImGui::CloseCurrentPopup();
                execute_pending_project_action();
            } catch (const std::exception& error) {
                set_status(error.what(), true);
            }
        } else if (result.status == ProjectTransitionStatus::Failed) {
            set_status(result.diagnostic, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
        static_cast<void>(resolve_unapplied_system_changes(
            application_state_, system_draft_ ? &*system_draft_ : nullptr,
            UnappliedSystemDecision::Discard));
        if (const auto* active_plan = application_state_.active_session().active_plan();
            active_plan != nullptr) {
            static_cast<void>(application_state_.active_session().replace_draft(*active_plan));
        }
        initialize_system_draft();
        ImGui::CloseCurrentPopup();
        execute_pending_project_action();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        static_cast<void>(resolve_unapplied_system_changes(
            application_state_, system_draft_ ? &*system_draft_ : nullptr,
            UnappliedSystemDecision::Cancel));
        pending_project_action_ = PendingProjectAction::None;
        pending_project_file_.clear();
        ImGui::CloseCurrentPopup();
    }
    if (!application_status_.empty() && application_status_error_) {
        ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.35F, 1.0F}, "%s", application_status_.c_str());
    }
    ImGui::EndPopup();
}

void GuiApplication::validate_system_draft() {
    if (!system_draft_.has_value() || !application_state_.has_active_project()) {
        set_status("System validation requires an active generic project.", true);
        return;
    }
    system_validation_ = system_draft_->build();
    if (!system_validation_.valid()) {
        set_status("System draft has " + std::to_string(system_validation_.diagnostics.size()) +
                       " issue(s).",
                   true);
        return;
    }
    const auto run_configuration =
        system_run_configuration(application_state_.active_session(), system_run_assignments_);
    const auto result = build_system_project_replacement(application_state_.active_project(),
                                                         *system_draft_, &run_configuration);
    if (result.valid()) {
        set_status("System and run configuration are valid; no active state was changed.", false);
    } else if (!result.run_plan_diagnostics.empty()) {
        set_status(result.run_plan_diagnostics.front().message, true);
    } else {
        set_status(result.diagnostic, true);
    }
}

void GuiApplication::apply_system_draft() {
    if (!system_draft_.has_value() || !application_state_.has_active_project()) {
        set_status("System changes require an active generic project.", true);
        return;
    }
    const auto run_configuration =
        system_run_configuration(application_state_.active_session(), system_run_assignments_);
    auto result =
        apply_system_project_draft(application_state_, *system_draft_, &run_configuration);
    if (result.valid()) {
        invalidate_completed_results();
        publish_complete_snapshot(false);
        progress_ = application_state_.active_session().progress();
        structural_selection_.select_system();
        runtime_selection_.clear();
        initialize_system_draft();
        set_status("System applied; the simulation session restarted paused.", false);
        return;
    }
    auto message = result.diagnostic;
    if (!result.system_diagnostics.empty()) {
        message += ": " + result.system_diagnostics.front().message;
    } else if (!result.run_plan_diagnostics.empty()) {
        message += ": " + result.run_plan_diagnostics.front().message;
    }
    set_status(std::move(message), true);
}

void GuiApplication::draw_project_dialog() {
    if (request_project_modal_) {
        ImGui::OpenPopup("Project location");
        request_project_modal_ = false;
    }
    if (!ImGui::BeginPopupModal("Project location", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const auto save_as = project_dialog_kind_ == ProjectDialogKind::SaveAs;
    ImGui::TextUnformatted(save_as ? "Save the active project under a new name."
                                   : "Create a minimal valid generic project.");
    ImGui::SetNextItemWidth(24.0F * ImGui::GetFontSize());
    ImGui::InputText("Project name", project_name_.data(), project_name_.size());
    auto parent_text = project_parent_.string();
    ImGui::SetNextItemWidth(24.0F * ImGui::GetFontSize());
    ImGui::InputText("Parent directory", parent_text.data(), parent_text.size() + 1,
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
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
                synchronize_project_workspace();
                capture_current_imgui_layout();
                const auto layout_writer = [this](const auto& root) {
                    if (imgui_layout_store_ != nullptr) {
                        imgui_layout_store_->write_current_to_project(root);
                    }
                };
                replacement = save_project_as(application_state_.active_project(), project_parent_,
                                              project_name_.data(), project_runtime_resolver(),
                                              layout_writer);
            } else {
                replacement = create_project(
                    make_generic_project_template(project_parent_, project_name_.data()));
            }
            replace_project(std::move(replacement));
            set_status(save_as ? "Project saved under its new name." : "Generic project created.",
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
        ImGui::InputText("Project name", bosch_project_name_.data(), bosch_project_name_.size());
        auto parent_text = bosch_parent_.string();
        ImGui::SetNextItemWidth(24.0F * ImGui::GetFontSize());
        ImGui::InputText("Parent directory", parent_text.data(), parent_text.size() + 1,
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
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
        const auto trajectory_name =
            bosch_trajectory_ == 3
                ? bosch_custom_trajectory_.string()
                : std::string{trajectories[static_cast<std::size_t>(bosch_trajectory_)]};
        ImGui::TextUnformatted("Review and Create");
        ImGui::Text("Trajectory: %s", trajectory_name.c_str());
        ImGui::Text("Scenario: %s", bosch_scenario_ == 0 ? "dedicated" : "shared_cloud");
        ImGui::Text("Horizon: %s",
                    bosch_complete_horizon_ ? "complete trajectory" : bosch_stop_tick_.data());
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
            const auto trajectory =
                bosch_trajectory_ == 3
                    ? bosch_custom_trajectory_
                    : paths_.examples_directory /
                          trajectories[static_cast<std::size_t>(bosch_trajectory_)];
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
    const auto visible = [this](GuiCenterTab tab) {
        switch (tab) {
        case GuiCenterTab::Architecture:
            return workspace_state_.panels.architecture;
        case GuiCenterTab::Timeline:
            return workspace_state_.panels.timeline;
        case GuiCenterTab::Signals:
            return workspace_state_.panels.signals;
        case GuiCenterTab::Results:
            return workspace_state_.panels.results;
        case GuiCenterTab::Resources:
            return workspace_state_.panels.resources;
        case GuiCenterTab::Events:
            return workspace_state_.panels.events;
        }
        return false;
    };
    const auto group_visible = [&visible](const std::vector<GuiCenterTab>& tabs) {
        return std::any_of(tabs.begin(), tabs.end(), visible);
    };
    const auto label = [](GuiCenterTab tab) {
        switch (tab) {
        case GuiCenterTab::Architecture:
            return "Architecture";
        case GuiCenterTab::Timeline:
            return "Scheduling Timeline";
        case GuiCenterTab::Signals:
            return "Functional Signals";
        case GuiCenterTab::Results:
            return "Results";
        case GuiCenterTab::Resources:
            return "Resources";
        case GuiCenterTab::Events:
            return "Canonical Events";
        }
        return "Unknown";
    };
    const auto draw_content = [&](GuiCenterTab tab) {
        switch (tab) {
        case GuiCenterTab::Architecture: {
            auto previewing = false;
            const auto* experiment = &snapshot.experiment;
            std::optional<ExperimentPresentationSnapshot> preview;
            if (system_changes_dirty() && system_draft_.has_value()) {
                system_validation_ = system_draft_->build();
                if (system_validation_.valid()) {
                    const auto* active_plan = session.active_plan();
                    auto request = RunPlanRequest{
                        .stop_tick = active_plan != nullptr ? active_plan->stop_tick() : 0,
                        .policy_kind = active_plan != nullptr ? active_plan->policy_kind()
                                                              : SchedulingPolicyKind::FixedPriority,
                        .assignments = {}};
                    for (const auto& assignment : system_run_assignments_) {
                        if (assignment.resource_id.has_value())
                            request.assignments.push_back(
                                {assignment.task_id, *assignment.resource_id});
                    }
                    const auto plan = build_run_plan(*system_validation_.config, request);
                    preview = plan.valid()
                                  ? build_experiment_presentation(*system_validation_.config,
                                                                  plan.plan->assignments())
                                  : build_experiment_presentation(*system_validation_.config);
                    experiment = &*preview;
                    previewing = true;
                    ImGui::TextColored(ImVec4{1.0F, 0.75F, 0.3F, 1.0F},
                                       "Previewing unapplied system changes");
                } else {
                    ImGui::TextDisabled(
                        "The active architecture is shown until the system draft is valid.");
                }
            }
            const auto functional_dependencies =
                application_state_.has_active_project() &&
                        application_state_.active_project().metadata().scenario_kind == "bosch"
                    ? bosch_functional_dependencies()
                    : std::vector<GuiFunctionalDependency>{};
            const auto is_bosch = application_state_.has_active_project() &&
                                  application_state_.active_project().metadata().scenario_kind ==
                                      "bosch";
            const auto graph =
                build_architecture_graph(*experiment, functional_dependencies, is_bosch,
                                         &workspace_state_.architecture);
            static_cast<void>(draw_architecture_view(
                graph, *experiment, system_run_assignments_, structural_selection_,
                workspace_state_.architecture, architecture_view_state_, session.draft_editable(),
                previewing, &pointer_regions_));
            break;
        }
        case GuiCenterTab::Timeline:
            {
                const auto region_min = ImGui::GetCursorScreenPos();
                const auto region_size = ImGui::GetContentRegionAvail();
                pointer_regions_.add(
                    {ImGui::GetID("Timeline interaction region"),
                     {region_min.x, region_min.y, region_min.x + region_size.x,
                      region_min.y + region_size.y},
                     GuiPointerRegionBehavior::PositionSensitive});
            }
            if (draw_timeline_view(snapshot, runtime_selection_, timeline_view_state_))
                workspace_state_.panels.inspector = true;
            break;
        case GuiCenterTab::Signals:
            draw_signal_view(snapshot, runtime_selection_, signal_view_state_);
            break;
        case GuiCenterTab::Results: {
            draw_results_view(progress_, completed_results_.get(), finalization_state(),
                              open_plot_visualizer_,
                              request_completed_export_, workspace_state_, results_view_state_,
                              &pointer_regions_);
            break;
        }
        case GuiCenterTab::Resources:
            draw_resource_view(snapshot, runtime_selection_);
            break;
        case GuiCenterTab::Events:
            draw_event_view(snapshot, publication_policy_.generations().presentation,
                            runtime_selection_, workspace_state_.event_filters,
                            workspace_state_.event_columns, event_view_state_, &profiler_);
            break;
        }
    };
    const auto draw_group = [&](const char* identity, const std::vector<GuiCenterTab>& tabs,
                                GuiCenterTab& active, bool upper) {
        if (!ImGui::BeginTabBar(identity, ImGuiTabBarFlags_Reorderable))
            return;
        for (const auto tab : tabs) {
            if (!visible(tab))
                continue;
            const auto flags = restore_center_tabs_ && active == tab ? ImGuiTabItemFlags_SetSelected
                                                                     : ImGuiTabItemFlags_None;
            const auto opened = ImGui::BeginTabItem(label(tab), nullptr, flags);
            ImGui::PushID(static_cast<int>(tab));
            if (ImGui::BeginPopupContextItem("Center tab menu")) {
                if (ImGui::MenuItem(upper ? "Move to Lower Panel" : "Move to Upper Panel")) {
                    move_center_tab(workspace_state_, tab, !upper);
                    restore_center_tabs_ = true;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            if (opened) {
                active = tab;
                draw_content(tab);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    };

    const auto upper_visible = group_visible(workspace_state_.upper_tabs);
    const auto lower_visible = group_visible(workspace_state_.lower_tabs);
    const auto available_height = ImGui::GetContentRegionAvail().y;
    if (upper_visible && lower_visible) {
        const auto splitter_height = horizontal_splitter_height();
        const auto split = calculate_vertical_split(available_height, splitter_height,
                                                    workspace_state_.center_split_ratio,
                                                    7.0F * ImGui::GetTextLineHeightWithSpacing(),
                                                    7.0F * ImGui::GetTextLineHeightWithSpacing());
        workspace_state_.center_split_ratio = split.normalized_ratio;
        ImGui::BeginChild("Upper center panel", ImVec2{0.0F, split.first_height},
                          ImGuiChildFlags_Borders);
        draw_group("Upper center tabs", workspace_state_.upper_tabs,
                   workspace_state_.active_upper_tab, true);
        ImGui::EndChild();
        draw_horizontal_splitter("Center panel splitter", available_height, splitter_height,
                                 workspace_state_.center_split_ratio, &pointer_regions_);
        ImGui::BeginChild("Lower center panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        draw_group("Lower center tabs", workspace_state_.lower_tabs,
                   workspace_state_.active_lower_tab, false);
        ImGui::EndChild();
    } else if (upper_visible) {
        ImGui::BeginChild("Upper center panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        draw_group("Upper center tabs", workspace_state_.upper_tabs,
                   workspace_state_.active_upper_tab, true);
        ImGui::EndChild();
    } else if (lower_visible) {
        ImGui::BeginChild("Lower center panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        draw_group("Lower center tabs", workspace_state_.lower_tabs,
                   workspace_state_.active_lower_tab, false);
        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("No center panels are visible. Use View to restore a panel.");
    }
    restore_center_tabs_ = false;
}

void GuiApplication::draw_left_sidebar(const SimulationSnapshot& snapshot) {
    const auto draw_explorer = [&] {
        ImGui::BeginChild("Experiment Explorer panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted("Experiment Explorer");
        ImGui::Separator();
        draw_experiment_explorer(
            snapshot.experiment, system_draft_.has_value() ? &*system_draft_ : nullptr,
            system_run_assignments_, structural_selection_, system_explorer_interaction_,
            application_state_.active_session().draft_editable(),
            application_state_.has_active_project()
                ? project_system_edit_policy(application_state_.active_project().metadata())
                : ProjectSystemEditPolicy::Generic,
            explorer_view_state_);
        ImGui::EndChild();
    };
    const auto draw_builder = [&] {
        ImGui::BeginChild("System Builder panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("System Builder");
        ImGui::Separator();
        if (application_state_.has_active_project() &&
            project_system_edit_policy(application_state_.active_project().metadata()) ==
                ProjectSystemEditPolicy::BoschCompatible) {
            const auto status = bosch_experiment_status(application_state_.active_project());
            ImGui::TextColored(status == BoschExperimentStatus::ReferenceBaseline
                                   ? ImVec4{0.45F, 0.85F, 0.55F, 1.0F}
                                   : ImVec4{1.0F, 0.75F, 0.3F, 1.0F},
                               "%s",
                               status == BoschExperimentStatus::ReferenceBaseline
                                   ? "Bosch reference baseline"
                                   : "Modified Bosch experiment");
        }
        if (system_draft_.has_value()) {
            synchronize_system_assignments();
            system_validation_ = system_draft_->build();
            if (explorer_view_state_.focus_request != SystemBuilderFocusTarget::None) {
                system_builder_view_state_.focus_request = explorer_view_state_.focus_request;
                explorer_view_state_.focus_request = SystemBuilderFocusTarget::None;
            }
            const auto project_name = application_state_.has_active_project()
                                          ? application_state_.active_project().metadata().name
                                          : std::string{"Standalone session"};
            draw_system_builder(
                *system_draft_, system_validation_, system_run_assignments_, structural_selection_,
                snapshot.experiment, application_state_.active_session().draft_editable(),
                application_state_.has_active_project()
                    ? project_system_edit_policy(application_state_.active_project().metadata())
                    : ProjectSystemEditPolicy::Generic,
                project_name, system_builder_view_state_);
        } else {
            ImGui::TextWrapped(
                "Structural editing is available for generic projects. This adapter-owned "
                "system remains read-only.");
        }
        ImGui::EndChild();
    };

    if (workspace_state_.panels.explorer && workspace_state_.panels.system_builder) {
        const auto available_height = ImGui::GetContentRegionAvail().y;
        const auto splitter_height = horizontal_splitter_height();
        const auto top_height = sidebar_top_height(available_height, splitter_height,
                                                   workspace_state_.left_sidebar_ratio);
        ImGui::BeginChild("Explorer upper", ImVec2{0.0F, top_height});
        draw_explorer();
        ImGui::EndChild();
        draw_horizontal_splitter("Explorer Builder splitter", available_height, splitter_height,
                                 workspace_state_.left_sidebar_ratio, &pointer_regions_);
        ImGui::BeginChild("Builder lower", ImVec2{0.0F, 0.0F});
        draw_builder();
        ImGui::EndChild();
    } else if (workspace_state_.panels.explorer) {
        draw_explorer();
    } else if (workspace_state_.panels.system_builder) {
        draw_builder();
    }
}

void GuiApplication::draw_right_sidebar(const SimulationSnapshot& snapshot) {
    const auto available_height = ImGui::GetContentRegionAvail().y;
    const auto splitter_height = horizontal_splitter_height();
    const auto top_height =
        sidebar_top_height(available_height, splitter_height, workspace_state_.right_sidebar_ratio);
    ImGui::BeginChild("Run Configuration upper", ImVec2{0.0F, top_height});
    ImGui::BeginChild("Run Configuration panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(system_changes_dirty() ? "Run Configuration — Unapplied changes"
                                                  : "Run Configuration");
    ImGui::Separator();
    synchronize_system_assignments();
    const auto action =
        draw_run_configuration(application_state_.active_session(), snapshot,
                               system_draft_.has_value() ? &*system_draft_ : nullptr,
                               system_run_assignments_, system_changes_dirty());
    if (action == RunConfigurationAction::ValidateChanges) {
        if (system_draft_.has_value()) {
            validate_system_draft_requested_ = true;
        } else {
            const auto& validation = application_state_.active_session().validate_draft();
            set_status(validation.valid() ? "Run configuration is valid."
                                          : validation.diagnostics.front().message,
                       !validation.valid());
        }
    } else if (action == RunConfigurationAction::ApplyAndRestart) {
        if (system_draft_.has_value()) {
            apply_system_draft_requested_ = true;
        } else if (application_state_.active_session().apply_draft()) {
            invalidate_completed_results();
            publish_complete_snapshot(false);
            progress_ = application_state_.active_session().progress();
            runtime_selection_.clear();
            set_status("Run configuration applied and restarted.", false);
        } else {
            set_status("Run configuration could not be applied.", true);
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
    draw_horizontal_splitter("Run Inspector splitter", available_height, splitter_height,
                             workspace_state_.right_sidebar_ratio, &pointer_regions_);
    ImGui::BeginChild("Runtime Inspector lower", ImVec2{0.0F, 0.0F});
    ImGui::BeginChild("Runtime Inspector panel", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Runtime Inspector");
    ImGui::Separator();
    draw_inspector_view(snapshot, runtime_selection_);
    ImGui::EndChild();
    ImGui::EndChild();
}

/*** Draws the existing shared-selection workbench for an active session. ***/
void GuiApplication::draw_workbench(const SimulationSnapshot& snapshot) {
    auto& session = application_state_.active_session();
    synchronize_selection(runtime_selection_, snapshot);
    if (!draw_main_menu()) {
        return;
    }

    const auto toolbar_height = 2.0F * ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Simulation toolbar", ImVec2{0.0F, toolbar_height}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    draw_toolbar(session, progress_, workspace_state_);
    const auto toolbar_min = ImGui::GetWindowPos();
    const auto toolbar_max = ImVec2{toolbar_min.x + ImGui::GetWindowWidth(),
                                    toolbar_min.y + ImGui::GetWindowHeight()};
    pointer_regions_.add({ImGui::GetID("Simulation toolbar region"),
                          {toolbar_min.x, toolbar_min.y, toolbar_max.x, toolbar_max.y},
                          GuiPointerRegionBehavior::BoundarySensitive});
    ImGui::EndChild();

    if (!application_status_.empty()) {
        const auto color = application_status_error_ ? ImVec4{1.0F, 0.45F, 0.35F, 1.0F}
                                                     : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", application_status_.c_str());
        ImGui::PopStyleColor();
    }

    const auto show_left_sidebar =
        workspace_state_.panels.explorer || workspace_state_.panels.system_builder;
    const auto column_count = 1 + static_cast<int>(show_left_sidebar) +
                              static_cast<int>(workspace_state_.panels.inspector);
    const auto layout_identity = static_cast<int>(show_left_sidebar) |
                                 (static_cast<int>(workspace_state_.panels.inspector) << 1);
    const auto available_width = ImGui::GetContentRegionAvail().x;
    const auto explorer_width = std::min(18.0F * ImGui::GetFontSize(), available_width * 0.24F);
    const auto inspector_width = std::min(28.0F * ImGui::GetFontSize(), available_width * 0.34F);
    ImGui::PushID(layout_identity);
    if (ImGui::BeginTable("Workbench layout", column_count,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp,
                          ImVec2{0.0F, 0.0F})) {
        if (show_left_sidebar) {
            ImGui::TableSetupColumn("Structure", ImGuiTableColumnFlags_WidthFixed, explorer_width);
        }
        ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch);
        if (workspace_state_.panels.inspector) {
            ImGui::TableSetupColumn("Run and runtime", ImGuiTableColumnFlags_WidthFixed,
                                    inspector_width);
        }
        ImGui::TableNextRow();

        int column = 0;
        if (show_left_sidebar) {
            ImGui::TableSetColumnIndex(column++);
            draw_left_sidebar(snapshot);
        }

        ImGui::TableSetColumnIndex(column++);
        draw_center_panels(snapshot);

        if (workspace_state_.panels.inspector) {
            ImGui::TableSetColumnIndex(column);
            draw_right_sidebar(snapshot);
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    synchronize_project_workspace();
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
        std::optional<std::filesystem::path> recent_to_open;
        for (const auto& entry : recent_projects_.entries()) {
            ImGui::PushID(entry.project_file.string().c_str());
            ImGui::SetCursorPosX(left);
            ImGui::BeginDisabled(!entry.available);
            auto recent_label = entry.project_file.parent_path().filename().string();
            if (!entry.available) {
                recent_label += " (unavailable)";
            }
            if (ImGui::Button(recent_label.c_str(), ImVec2{scaled_button_width * 0.78F, 0.0F})) {
                recent_to_open = entry.project_file;
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
        if (recent_to_open.has_value()) {
            try {
                auto replacement = load_project(*recent_to_open, project_runtime_resolver());
                replace_project(std::move(replacement));
                record_active_project();
            } catch (const std::exception& error) {
                set_status(error.what(), true);
            }
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
    pointer_regions_.begin_frame();
    apply_pending_imgui_layout();
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
        if (presentation_snapshot_ == nullptr) {
            publish_complete_snapshot(false);
        }
        draw_workbench(*presentation_snapshot_);
    }

    ImGui::End();
    if (application_state_.has_active_session()) {
        draw_about_dialog();
    }
    draw_unapplied_changes_dialog();
    draw_project_dialog();
    draw_bosch_wizard();
    draw_export_dialog();
    draw_plot_visualizer(open_plot_visualizer_, completed_results_.get(), workspace_state_,
                         runtime_selection_, plot_visualizer_view_state_, &pointer_regions_,
                         &profiler_);
    if (validate_system_draft_requested_) {
        validate_system_draft_requested_ = false;
        validate_system_draft();
    }
    if (apply_system_draft_requested_) {
        apply_system_draft_requested_ = false;
        apply_system_draft();
    }
    if (request_completed_export_) {
        request_completed_export_ = false;
        const auto run_id = default_export_run_id();
        export_run_id_.fill('\0');
        std::copy_n(run_id.begin(), std::min(run_id.size(), export_run_id_.size() - 1),
                    export_run_id_.begin());
        export_destination_ = application_state_.active_project().root() / "results";
        export_scope_ = 0;
        export_diagnostic_.clear();
        open_export_dialog_ = true;
    }
#if !defined(NDEBUG)
    if (open_profiler_) {
        if (ImGui::Begin("GUI Profiler", &open_profiler_)) {
            const auto values = profiler_.snapshot();
            static constexpr const char* counter_names[]{
                "Poll",          "Timed wait",       "Indefinite wait", "Rendered frames",
                "Skipped frames", "Background wakes", "Snapshots",       "Results",
                "Event rows",    "Event filters",    "Plot caches",     "Bosch analyses"};
            for (std::size_t index = 0; index < values.counters.size(); ++index) {
                ImGui::Text("%s: %llu", counter_names[index],
                            static_cast<unsigned long long>(values.counters[index]));
            }
            if (ImGui::Button("Reset counters")) {
                profiler_.reset();
            }
        }
        ImGui::End();
    }
#endif
    pointer_regions_.publish();
}

} // namespace cpssim::gui
