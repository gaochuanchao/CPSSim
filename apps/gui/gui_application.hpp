/*** Own the optional CPSSim project/session shell and Goal 1 workflows. ***/

#pragma once

#include "views/architecture_view.hpp"
#include "views/experiment_explorer.hpp"
#include "views/signal_view.hpp"
#include "views/system_builder.hpp"
#include "views/timeline_view.hpp"

#include "cpssim/application/file_dialog.hpp"
#include "cpssim/application/recent_projects.hpp"
#include "cpssim/gui/application_state.hpp"
#include "cpssim/gui/selection_model.hpp"

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

    bool draw_main_menu();
    void draw_home_screen();
    void draw_workbench(const SimulationSnapshot& snapshot);
    void draw_about_dialog();
    void draw_project_dialog();
    void draw_bosch_wizard();
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
    bool show_explorer_{true};
    bool show_inspector_{true};
    bool show_architecture_{true};
    bool show_system_builder_{true};
    bool show_timeline_{true};
    bool show_signals_{true};
    bool show_resources_{true};
    bool show_events_{true};
    bool open_about_{false};
    bool request_project_modal_{false};
    ProjectDialogKind project_dialog_kind_{ProjectDialogKind::None};
    std::array<char, 256> project_name_{};
    std::filesystem::path project_parent_;
    bool open_bosch_wizard_{false};
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
    float left_sidebar_ratio_{0.52F};
    float right_sidebar_ratio_{0.48F};
    float text_scale_{1.0F};
};

} // namespace cpssim::gui
