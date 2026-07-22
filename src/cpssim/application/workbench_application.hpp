/*** Graphics-independent application owner shared by CPSSim GUI frontends. ***/
#pragma once

#include "cpssim/analysis/completed_run_finalizer.hpp"
#include "cpssim/analysis/completed_run_result.hpp"
#include "cpssim/application/project/project.hpp"
#include "cpssim/application/project/system_builder_workflow.hpp"
#include "cpssim/application/project/system_edit_policy.hpp"
#include "cpssim/application/recent_projects.hpp"
#include "cpssim/application/result_export.hpp"
#include "cpssim/gui/application_state.hpp"
#include "cpssim/gui/presentation_publication.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/system_builder_interaction.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

struct WorkbenchApplicationPaths {
    std::filesystem::path projects_directory{"projects"};
    std::filesystem::path preferences_file;
};

struct WorkbenchApplicationServices {
    ProjectRuntimeResolver project_runtime_resolver;
    CompletedRunFinalizationBuilder completed_result_builder;
};

/***
 * Owns all non-rendering workbench state. Frontends retain only toolkit view
 * state and translate user gestures into this class's public operations.
 ***/
class WorkbenchApplication {
  public:
    explicit WorkbenchApplication(WorkbenchApplicationPaths paths = {},
                                  WorkbenchApplicationServices services = {});
    WorkbenchApplication(std::unique_ptr<GuiSimulationSession> session,
                         WorkbenchApplicationPaths paths = {},
                         WorkbenchApplicationServices services = {});
    WorkbenchApplication(std::unique_ptr<ProjectContext> project,
                         WorkbenchApplicationPaths paths = {},
                         WorkbenchApplicationServices services = {});
    ~WorkbenchApplication();

    WorkbenchApplication(const WorkbenchApplication&) = delete;
    WorkbenchApplication& operator=(const WorkbenchApplication&) = delete;

    GuiApplicationScreen screen() const noexcept { return application_state_.screen(); }
    bool has_active_session() const noexcept { return application_state_.has_active_session(); }
    bool has_active_project() const noexcept { return application_state_.has_active_project(); }
    GuiRunState run_state() const noexcept;
    bool has_queued_work() const noexcept;
    bool needs_update() const noexcept;

    GuiApplicationState& application_state() noexcept { return application_state_; }
    const GuiApplicationState& application_state() const noexcept { return application_state_; }
    GuiSimulationSession& active_session() { return application_state_.active_session(); }
    const GuiSimulationSession& active_session() const {
        return application_state_.active_session();
    }
    ProjectContext& active_project() { return application_state_.active_project(); }
    const ProjectContext& active_project() const { return application_state_.active_project(); }

    StructuralSelection& structural_selection() noexcept { return structural_selection_; }
    const StructuralSelection& structural_selection() const noexcept {
        return structural_selection_;
    }
    GuiSelection& runtime_selection() noexcept { return runtime_selection_; }
    const GuiSelection& runtime_selection() const noexcept { return runtime_selection_; }
    SystemExplorerInteraction& explorer_interaction() noexcept { return explorer_interaction_; }
    GuiWorkspaceState& workspace() noexcept { return workspace_state_; }
    const GuiWorkspaceState& workspace() const noexcept { return workspace_state_; }
    RecentProjects& recent_projects() noexcept { return recent_projects_; }
    const RecentProjects& recent_projects() const noexcept { return recent_projects_; }

    const SimulationProgress& progress() const noexcept { return progress_; }
    std::shared_ptr<const SimulationSnapshot> presentation_snapshot() const noexcept {
        return presentation_snapshot_;
    }
    const std::shared_ptr<const SimulationSnapshot>& presentation_snapshot_ref() const noexcept {
        return presentation_snapshot_;
    }
    std::uint64_t presentation_generation() const noexcept {
        return publication_policy_.generations().presentation;
    }
    const CompletedRunResult* completed_result() const noexcept { return completed_results_.get(); }
    CompletedResultFinalizationState finalization_state() const noexcept;
    bool background_pending() const noexcept;
    void set_background_wakeup(std::function<void()> wakeup);
    bool process_background_publications();
    void cancel_background_work();

    std::optional<EditableSystemDraft>& editable_system() noexcept { return system_draft_; }
    const std::optional<EditableSystemDraft>& editable_system() const noexcept {
        return system_draft_;
    }
    SystemDraftBuildResult& system_validation() noexcept { return system_validation_; }
    const SystemDraftBuildResult& system_validation() const noexcept { return system_validation_; }
    std::vector<DraftTaskAssignment>& run_assignments() noexcept { return run_assignments_; }
    const std::vector<DraftTaskAssignment>& run_assignments() const noexcept {
        return run_assignments_;
    }
    bool system_changes_dirty() const;
    void synchronize_system_assignments();
    void initialize_system_draft();
    void validate_system_draft();
    bool apply_system_draft();
    bool set_task_assignment(TaskId task_id, std::optional<ResourceId> resource_id);

    bool enqueue(GuiCommand command);
    bool update();
    void active_runtime_replaced();
    void publish_complete_snapshot(bool publish_finished_result);
    void invalidate_completed_results();

    void replace_session(std::unique_ptr<GuiSimulationSession> session);
    void replace_project(std::unique_ptr<ProjectContext> project);
    void create_project(ProjectCreationRequest request, ProjectRuntimeInputs runtime_inputs = {},
                        const ProjectContentWriter& content_writer = {});
    void open_project(const std::filesystem::path& project_file);
    void save_project();
    void save_project_as(const std::filesystem::path& parent_directory, std::string new_name,
                         const ProjectContentWriter& content_writer = {});
    void close_project();
    ProjectTransitionResult resolve_unapplied_changes(UnappliedSystemDecision decision);
    RunExportArtifacts export_completed_result(const RunExportOptions& options) const;

    void synchronize_project_workspace();
    void record_active_project();
    void persist_recent_history();
    ProjectRuntimeResolver project_runtime_resolver() const {
        return services_.project_runtime_resolver;
    }
    const WorkbenchApplicationPaths& paths() const noexcept { return paths_; }

    const std::string& status() const noexcept { return status_; }
    bool status_is_error() const noexcept { return status_is_error_; }
    void set_status(std::string message, bool error = false);
    void clear_status();

  private:
    void initialize();
    void load_workspace_state();
    void load_recent_history();
    SystemRunConfigurationDraft system_run_configuration() const;

    WorkbenchApplicationPaths paths_;
    WorkbenchApplicationServices services_;
    GuiApplicationState application_state_;
    RecentProjects recent_projects_;
    StructuralSelection structural_selection_;
    GuiSelection runtime_selection_;
    SystemExplorerInteraction explorer_interaction_;
    GuiWorkspaceState workspace_state_;
    std::shared_ptr<const SimulationSnapshot> presentation_snapshot_;
    GuiPresentationPublicationPolicy publication_policy_;
    SimulationProgress progress_;
    GuiRunMode last_run_mode_{GuiRunMode::Live};
    CompletedRunResultCache completed_results_;
    std::unique_ptr<CompletedRunFinalizer> completed_finalizer_;
    std::optional<EditableSystemDraft> system_draft_;
    SystemDraftBuildResult system_validation_;
    std::vector<DraftTaskAssignment> run_assignments_;
    std::string status_;
    bool status_is_error_{false};
};

} // namespace cpssim
