/*** Own the optional CPSSim project/session shell and Goal 1 workflows. ***/

#pragma once

#include "views/architecture_view.hpp"
#include "views/event_view.hpp"
#include "views/experiment_explorer.hpp"
#include "views/resource_view.hpp"
#include "views/results_view.hpp"
#include "views/signal_view.hpp"
#include "views/system_builder.hpp"
#include "views/timeline_view.hpp"

#include "cpssim/application/file_dialog.hpp"
#include "cpssim/application/recent_projects.hpp"
#include "cpssim/gui/application_state.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace cpssim::gui {

struct GuiApplicationPaths {
    std::filesystem::path projects_directory{"projects"};
    std::filesystem::path preferences_file;
    std::filesystem::path examples_directory{"examples"};
    std::filesystem::path bosch_reference_directory{"experiments/bosch_v10_reference"};
    std::filesystem::path bosch_fmu_library;
};

class GuiApplication {
  public:
    GuiApplication();
    GuiApplication(std::unique_ptr<GuiSimulationSession> session,
                   std::unique_ptr<FileDialog> dialogs, GuiApplicationPaths paths);
    GuiApplication(std::unique_ptr<ProjectContext> project, std::unique_ptr<FileDialog> dialogs,
                   GuiApplicationPaths paths);

    GuiApplicationScreen screen() const noexcept { return application_state_.screen(); }
    bool has_active_session() const noexcept { return application_state_.has_active_session(); }
    bool has_active_project() const noexcept { return application_state_.has_active_project(); }
    GuiTheme theme() const noexcept { return workspace_state_.theme; }
    void replace_session(std::unique_ptr<GuiSimulationSession> session);
    void replace_project(std::unique_ptr<ProjectContext> project);
    void load_project_file(const std::filesystem::path& project_file);
    void save_active_project();
    void clear_session();
    void update_active_session();
    void draw_frame();

  private:
    enum class ProjectDialogKind { None, NewGeneric, SaveAs };
    enum class BoschWizardStep { Trajectory, Scenario, Horizon, Project, Review };
    enum class PendingProjectAction {
        None,
        CreateGeneric,
        OpenDialog,
        OpenRecent,
        BoschWizard,
        SaveAs,
        Close,
    };

    void initialize_presentation_state();
    void initialize_system_draft();
    bool system_changes_dirty() const;
    ProjectRuntimeResolver project_runtime_resolver() const;
    void record_active_project();
    void persist_recents();
    void set_status(std::string message, bool error);
    void open_project_dialog();
    void load_run_plan_dialog();
    void save_run_plan_dialog();
    void request_project_action(PendingProjectAction action,
                                std::filesystem::path project_file = {});
    bool execute_pending_project_action();
    void draw_unapplied_changes_dialog();
    void validate_system_draft();
    void apply_system_draft();
    void synchronize_system_assignments();
    void load_workspace_state();
    void synchronize_project_workspace();

    bool draw_main_menu();
    void draw_home_screen();
    void draw_workbench(const SimulationSnapshot& snapshot);
    void draw_about_dialog();
    void draw_project_dialog();
    void draw_bosch_wizard();
    void draw_export_dialog();
    void draw_center_panels(const SimulationSnapshot& snapshot);
    void draw_left_sidebar(const SimulationSnapshot& snapshot);
    void draw_right_sidebar(const SimulationSnapshot& snapshot);

    GuiApplicationState application_state_;
    std::unique_ptr<FileDialog> dialogs_;
    GuiApplicationPaths paths_;
    RecentProjects recent_projects_;
    StructuralSelection structural_selection_;
    GuiSelection runtime_selection_;
    SystemExplorerInteraction system_explorer_interaction_;
    ExperimentExplorerViewState explorer_view_state_;
    GuiWorkspaceState workspace_state_;
    bool open_about_{false};
    bool request_project_modal_{false};
    ProjectDialogKind project_dialog_kind_{ProjectDialogKind::None};
    std::array<char, 256> project_name_{};
    std::filesystem::path project_parent_;
    bool open_bosch_wizard_{false};
    bool open_export_dialog_{false};
    std::array<char, 128> export_run_id_{};
    std::filesystem::path export_destination_;
    int export_scope_{0};
    bool export_excel_{true};
    std::string export_diagnostic_;
    BoschWizardStep bosch_step_{BoschWizardStep::Trajectory};
    int bosch_trajectory_{0};
    std::filesystem::path bosch_custom_trajectory_;
    int bosch_scenario_{0};
    bool bosch_complete_horizon_{true};
    std::array<char, 64> bosch_stop_tick_{};
    std::array<char, 256> bosch_project_name_{};
    std::filesystem::path bosch_parent_;
    std::string application_status_;
    bool application_status_error_{false};
    std::optional<EditableSystemDraft> system_draft_;
    SystemDraftBuildResult system_validation_;
    std::vector<DraftTaskAssignment> system_run_assignments_;
    SystemBuilderViewState system_builder_view_state_;
    bool validate_system_draft_requested_{false};
    bool apply_system_draft_requested_{false};
    PendingProjectAction pending_project_action_{PendingProjectAction::None};
    std::filesystem::path pending_project_file_;
    bool request_unapplied_modal_{false};
    ArchitectureViewState architecture_view_state_;
    TimelineViewState timeline_view_state_;
    SignalViewState signal_view_state_;
    ResourceViewState resource_view_state_;
    ResultsViewState results_view_state_;
    EventViewState event_view_state_;
    bool restore_analysis_tab_{true};
    float text_scale_{1.0F};
};

} // namespace cpssim::gui
