/*** Implement the graphics-independent CPSSim workbench owner. ***/
#include "cpssim/application/workbench_application.hpp"

#include "cpssim/analysis/run_result.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace cpssim {
namespace {

CompletedRunFinalizationBuilder default_completed_result_builder() {
    return [](const CompletedRunFinalizationRequest& request, std::stop_token stop) {
        if (stop.stop_requested()) {
            return CompletedRunResult{};
        }
        const auto started = std::chrono::steady_clock::now();
        auto result = std::make_shared<const RunResult>(
            build_run_result(request.data, request.scenario_kind));
        return CompletedRunResult{request.data->runtime_generation, std::move(result), nullptr,
                                  request.performance, std::chrono::steady_clock::now() - started};
    };
}

} // namespace

WorkbenchApplication::WorkbenchApplication(WorkbenchApplicationPaths paths,
                                           WorkbenchApplicationServices services)
    : paths_{std::move(paths)}, services_{std::move(services)} {
    initialize();
}

WorkbenchApplication::WorkbenchApplication(std::unique_ptr<GuiSimulationSession> session,
                                           WorkbenchApplicationPaths paths,
                                           WorkbenchApplicationServices services)
    : paths_{std::move(paths)}, services_{std::move(services)} {
    if (session != nullptr) {
        application_state_.replace_session(std::move(session));
    }
    initialize();
}

WorkbenchApplication::WorkbenchApplication(std::unique_ptr<ProjectContext> project,
                                           WorkbenchApplicationPaths paths,
                                           WorkbenchApplicationServices services)
    : paths_{std::move(paths)}, services_{std::move(services)} {
    if (project != nullptr) {
        application_state_.replace_project(std::move(project));
    }
    initialize();
}

WorkbenchApplication::~WorkbenchApplication() { cancel_background_work(); }

void WorkbenchApplication::initialize() {
    if (!services_.completed_result_builder) {
        services_.completed_result_builder = default_completed_result_builder();
    }
    completed_finalizer_ =
        std::make_unique<CompletedRunFinalizer>(services_.completed_result_builder);
    structural_selection_.select_system();
    runtime_selection_.clear();
    load_recent_history();
    load_workspace_state();
    initialize_system_draft();
    if (has_active_session()) {
        publish_complete_snapshot(false);
        progress_ = active_session().progress();
    }
    last_run_mode_ = workspace_state_.run_mode;
}

GuiRunState WorkbenchApplication::run_state() const noexcept {
    return has_active_session() ? active_session().run_state() : GuiRunState::NotConfigured;
}

bool WorkbenchApplication::has_queued_work() const noexcept {
    return has_active_session() && active_session().has_queued_work();
}

bool WorkbenchApplication::needs_update() const noexcept {
    return has_active_session() && active_session().needs_update();
}

CompletedResultFinalizationState WorkbenchApplication::finalization_state() const noexcept {
    return completed_finalizer_ != nullptr ? completed_finalizer_->state()
                                           : CompletedResultFinalizationState::Idle;
}

bool WorkbenchApplication::background_pending() const noexcept {
    return finalization_state() == CompletedResultFinalizationState::Finalizing;
}

void WorkbenchApplication::set_background_wakeup(std::function<void()> wakeup) {
    completed_finalizer_->set_wakeup(std::move(wakeup));
}

bool WorkbenchApplication::process_background_publications() {
    if (completed_finalizer_ == nullptr || !completed_finalizer_->publication_pending()) {
        return false;
    }
    if (auto completed = completed_finalizer_->take_publication(); completed.has_value()) {
        static_cast<void>(completed_results_.publish_ready(std::move(*completed)));
        return true;
    }
    if (const auto diagnostic = completed_finalizer_->diagnostic(); diagnostic.has_value()) {
        set_status("Completed-run finalization failed: " + *diagnostic, true);
        return true;
    }
    return false;
}

void WorkbenchApplication::cancel_background_work() {
    if (completed_finalizer_ != nullptr) {
        completed_finalizer_->set_wakeup({});
        completed_finalizer_->cancel();
    }
}

void WorkbenchApplication::invalidate_completed_results() {
    if (completed_finalizer_ != nullptr) {
        completed_finalizer_->reset();
    }
    completed_results_.invalidate();
}

void WorkbenchApplication::load_recent_history() {
    if (paths_.preferences_file.empty()) {
        return;
    }
    auto loaded = load_recent_projects(paths_.preferences_file);
    recent_projects_ = std::move(loaded.recent);
    if (loaded.diagnostic.has_value()) {
        set_status(*loaded.diagnostic, true);
    }
}

void WorkbenchApplication::persist_recent_history() {
    if (paths_.preferences_file.empty()) {
        return;
    }
    try {
        save_recent_projects(paths_.preferences_file, recent_projects_);
    } catch (const std::exception& error) {
        set_status(error.what(), true);
    }
}

void WorkbenchApplication::record_active_project() {
    if (!has_active_project()) {
        return;
    }
    recent_projects_.add(active_project().root() / "project.json");
    persist_recent_history();
    if (active_project().workspace_diagnostic().has_value()) {
        set_status(*active_project().workspace_diagnostic(), true);
    }
}

void WorkbenchApplication::load_workspace_state() {
    workspace_state_ = has_active_project() ? active_project().workspace() : GuiWorkspaceState{};
    normalize_workspace_state(workspace_state_);
}

void WorkbenchApplication::synchronize_project_workspace() {
    normalize_workspace_state(workspace_state_);
    if (has_active_project()) {
        active_project().set_workspace(workspace_state_);
    }
}

void WorkbenchApplication::initialize_system_draft() {
    system_draft_.reset();
    system_validation_ = {};
    run_assignments_.clear();
    if (!has_active_project() || project_system_edit_policy(active_project().metadata()) ==
                                     ProjectSystemEditPolicy::ReadOnlyAdapter) {
        return;
    }
    system_draft_.emplace(active_session().config());
    system_validation_ = system_draft_->build();
    if (const auto* plan = active_session().active_plan(); plan != nullptr) {
        run_assignments_.reserve(plan->assignments().size());
        for (const auto& assignment : plan->assignments()) {
            run_assignments_.push_back(
                {.task_id = assignment.task_id, .resource_id = assignment.resource_id});
        }
    }
}

bool WorkbenchApplication::system_changes_dirty() const {
    if (!system_draft_.has_value()) {
        return false;
    }
    if (system_draft_->dirty() || active_session().draft_dirty()) {
        return true;
    }
    const auto* active = active_session().active_plan();
    if (active == nullptr || active->assignments().size() != run_assignments_.size()) {
        return true;
    }
    for (const auto& assignment : run_assignments_) {
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

void WorkbenchApplication::synchronize_system_assignments() {
    if (!system_draft_.has_value()) {
        return;
    }
    std::vector<DraftTaskAssignment> synchronized;
    synchronized.reserve(system_draft_->tasks().size());
    for (const auto& task : system_draft_->tasks()) {
        const auto found =
            std::find_if(run_assignments_.begin(), run_assignments_.end(),
                         [&task](const auto& row) { return row.task_id == task.id; });
        synchronized.push_back(
            {.task_id = task.id,
             .resource_id = found == run_assignments_.end() ? std::nullopt : found->resource_id});
    }
    run_assignments_ = std::move(synchronized);
}

SystemRunConfigurationDraft WorkbenchApplication::system_run_configuration() const {
    return {.stop_tick = active_session().draft().stop_tick(),
            .policy_kind = active_session().draft().policy_kind(),
            .assignments = run_assignments_};
}

void WorkbenchApplication::validate_system_draft() {
    if (!system_draft_.has_value() || !has_active_project()) {
        set_status("System validation requires an active editable project.", true);
        return;
    }
    system_validation_ = system_draft_->build();
    if (!system_validation_.valid()) {
        set_status("System draft has " + std::to_string(system_validation_.diagnostics.size()) +
                       " issue(s).",
                   true);
        return;
    }
    const auto run_configuration = system_run_configuration();
    const auto result =
        build_system_project_replacement(active_project(), *system_draft_, &run_configuration);
    if (result.valid()) {
        set_status("System and run configuration are valid; no active state was changed.");
    } else if (!result.run_plan_diagnostics.empty()) {
        set_status(result.run_plan_diagnostics.front().message, true);
    } else {
        set_status(result.diagnostic, true);
    }
}

bool WorkbenchApplication::apply_system_draft() {
    if (!system_draft_.has_value() || !has_active_project()) {
        set_status("System changes require an active editable project.", true);
        return false;
    }
    const auto run_configuration = system_run_configuration();
    auto result =
        apply_system_project_draft(application_state_, *system_draft_, &run_configuration);
    if (!result.valid()) {
        auto message = result.diagnostic;
        if (!result.system_diagnostics.empty()) {
            message += ": " + result.system_diagnostics.front().message;
        } else if (!result.run_plan_diagnostics.empty()) {
            message += ": " + result.run_plan_diagnostics.front().message;
        }
        set_status(std::move(message), true);
        return false;
    }
    invalidate_completed_results();
    publish_complete_snapshot(false);
    progress_ = active_session().progress();
    structural_selection_.select_system();
    runtime_selection_.clear();
    initialize_system_draft();
    set_status("System applied; the simulation session restarted paused.");
    return true;
}

bool WorkbenchApplication::set_task_assignment(TaskId task_id,
                                               std::optional<ResourceId> resource_id) {
    if (!system_draft_.has_value() || run_state() == GuiRunState::Running) {
        set_status("Pause the simulation before editing resource assignments.", true);
        return false;
    }
    const auto task = std::find_if(system_draft_->tasks().begin(), system_draft_->tasks().end(),
                                   [task_id](const auto& row) { return row.id == task_id; });
    if (task == system_draft_->tasks().end()) {
        set_status("The selected task is unavailable.", true);
        return false;
    }
    if (resource_id.has_value()) {
        const auto resource =
            std::find_if(system_draft_->resources().begin(), system_draft_->resources().end(),
                         [resource_id](const auto& row) { return row.id == *resource_id; });
        if (resource == system_draft_->resources().end()) {
            set_status("The selected resource is unavailable.", true);
            return false;
        }
    }
    const auto assignment =
        std::find_if(run_assignments_.begin(), run_assignments_.end(),
                     [task_id](const auto& row) { return row.task_id == task_id; });
    if (assignment == run_assignments_.end()) {
        set_status("The task has no editable run assignment row.", true);
        return false;
    }
    assignment->resource_id = resource_id;
    structural_selection_.select_task(task_id);
    validate_system_draft();
    return true;
}

void WorkbenchApplication::restore_system_draft(EditableSystemDraft draft,
                                                std::vector<DraftTaskAssignment> assignments,
                                                StructuralSelection selection) {
    if (!has_active_project() || run_state() == GuiRunState::Running) {
        throw std::logic_error{"system draft restoration requires a paused editable project"};
    }
    system_draft_ = std::move(draft);
    run_assignments_ = std::move(assignments);
    structural_selection_ = std::move(selection);
    synchronize_system_assignments();
    validate_system_draft();
}

bool WorkbenchApplication::enqueue(GuiCommand command) {
    return has_active_session() && active_session().enqueue(command);
}

void WorkbenchApplication::publish_complete_snapshot(bool publish_finished_result) {
    auto& session = active_session();
    auto snapshot = session.snapshot();
    if (publish_finished_result && snapshot.run_state == GuiRunState::Finished) {
        auto data = std::make_shared<const CompletedRunData>(
            CompletedRunData{session.runtime_generation(), session.simulation_data_generation(),
                             std::move(snapshot)});
        presentation_snapshot_ = std::shared_ptr<const SimulationSnapshot>{data, &data->snapshot};
        const auto scenario = has_active_project() ? active_project().metadata().scenario_kind
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

bool WorkbenchApplication::update() {
    if (!needs_update()) {
        return false;
    }
    const auto previous = progress_;
    auto& session = active_session();
    const auto settings =
        GuiExecutionSettings{.mode = workspace_state_.run_mode,
                             .batch_unit = workspace_state_.fast_batch_unit,
                             .event_batch_size = workspace_state_.fast_event_batch_size,
                             .tick_batch_size = workspace_state_.fast_tick_batch_size};
    const auto update_result = session.update(settings);
    progress_ = session.progress();
    const auto switched_to_live =
        last_run_mode_ == GuiRunMode::Fast && workspace_state_.run_mode == GuiRunMode::Live;
    const auto publication = GuiPresentationPublicationInput{
        .mode = workspace_state_.run_mode,
        .update = update_result,
        .switched_fast_to_live = switched_to_live,
        .missing_snapshot = presentation_snapshot_ == nullptr,
        .runtime_generation = session.runtime_generation(),
        .simulation_data_generation = session.simulation_data_generation(),
        .now = std::chrono::steady_clock::now()};
    if (publication_policy_.should_publish(publication)) {
        publish_complete_snapshot(update_result.finished);
    }
    if (update_result.reset) {
        invalidate_completed_results();
    }
    last_run_mode_ = workspace_state_.run_mode;
    return update_result.reset || update_result.paused || update_result.finished ||
           update_result.transitions != 0 || progress_.run_state != previous.run_state ||
           progress_.current_tick != previous.current_tick ||
           progress_.event_count != previous.event_count;
}

void WorkbenchApplication::active_runtime_replaced() {
    invalidate_completed_results();
    publish_complete_snapshot(false);
    progress_ = active_session().progress();
    runtime_selection_.clear();
}

void WorkbenchApplication::replace_session(std::unique_ptr<GuiSimulationSession> session) {
    invalidate_completed_results();
    application_state_.replace_session(std::move(session));
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    clear_status();
    publish_complete_snapshot(false);
    progress_ = active_session().progress();
    last_run_mode_ = workspace_state_.run_mode;
}

void WorkbenchApplication::replace_project(std::unique_ptr<ProjectContext> project) {
    invalidate_completed_results();
    application_state_.replace_project(std::move(project));
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    clear_status();
    publish_complete_snapshot(false);
    progress_ = active_session().progress();
    last_run_mode_ = workspace_state_.run_mode;
}

void WorkbenchApplication::create_project(ProjectCreationRequest request,
                                          ProjectRuntimeInputs runtime_inputs,
                                          const ProjectContentWriter& content_writer) {
    replace_project(cpssim::create_project(request, std::move(runtime_inputs), content_writer));
    record_active_project();
}

void WorkbenchApplication::open_project(const std::filesystem::path& project_file) {
    auto replacement = load_project(project_file, services_.project_runtime_resolver);
    replace_project(std::move(replacement));
    record_active_project();
}

void WorkbenchApplication::save_project() {
    if (!has_active_project()) {
        throw std::logic_error{"the workbench has no project to save"};
    }
    synchronize_project_workspace();
    cpssim::save_project(active_project());
}

void WorkbenchApplication::save_project_as(const std::filesystem::path& parent_directory,
                                           std::string new_name,
                                           const ProjectContentWriter& content_writer) {
    if (!has_active_project()) {
        throw std::logic_error{"the workbench has no project to save"};
    }
    synchronize_project_workspace();
    auto replacement =
        cpssim::save_project_as(active_project(), parent_directory, std::move(new_name),
                                services_.project_runtime_resolver, content_writer);
    replace_project(std::move(replacement));
    record_active_project();
}

void WorkbenchApplication::close_project() {
    invalidate_completed_results();
    application_state_.clear_session();
    load_workspace_state();
    initialize_system_draft();
    structural_selection_.select_system();
    runtime_selection_.clear();
    clear_status();
    presentation_snapshot_.reset();
    progress_ = {};
}

ProjectTransitionResult
WorkbenchApplication::resolve_unapplied_changes(UnappliedSystemDecision decision) {
    const auto configuration =
        system_draft_.has_value() ? std::optional{system_run_configuration()} : std::nullopt;
    return cpssim::resolve_unapplied_system_changes(
        application_state_, system_draft_ ? &*system_draft_ : nullptr, decision,
        configuration.has_value() ? &*configuration : nullptr);
}

ProjectTransitionResult WorkbenchApplication::apply_and_save_project() {
    if (!system_draft_.has_value() || !has_active_project()) {
        // No draft; just synchronize workspace and save.
        synchronize_project_workspace();
        cpssim::save_project(active_project());
        return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
    }

    synchronize_project_workspace();

    const auto result = resolve_unapplied_changes(UnappliedSystemDecision::ApplyAndSave);
    if (result.status == ProjectTransitionStatus::Failed) {
        return result;
    }

    // After successful replacement, reinitialize all cached state from the
    // now-active project so the draft baseline matches the saved config.
    initialize_system_draft();
    invalidate_completed_results();
    publish_complete_snapshot(false);

    return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
}

RunExportArtifacts
WorkbenchApplication::export_completed_result(const RunExportOptions& options) const {
    if (!has_active_project() || completed_results_.get() == nullptr ||
        completed_results_.get()->result == nullptr) {
        throw std::logic_error{"completed project results are unavailable"};
    }
    return export_run_result(active_project(), *completed_results_.get()->result, options);
}

void WorkbenchApplication::set_status(std::string message, bool error) {
    status_ = std::move(message);
    status_is_error_ = error;
}

void WorkbenchApplication::clear_status() {
    status_.clear();
    status_is_error_ = false;
}

} // namespace cpssim
