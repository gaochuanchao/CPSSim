/*** Drive CPSSim cooperatively from the Qt GUI thread. ***/
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/config/json_run_plan.hpp"

#include <QMetaObject>
#include <QPointer>

#include <stdexcept>
#include <utility>

namespace cpssim::qt {

QtWorkbenchBridge::QtWorkbenchBridge(std::unique_ptr<WorkbenchApplication> application,
                                     QObject* parent)
    : QObject(parent), application_{std::move(application)} {
    if (application_ == nullptr) {
        throw std::invalid_argument{"Qt workbench bridge requires an application"};
    }
    live_timer_.setParent(this);
    live_timer_.setInterval(16);
    live_timer_.setTimerType(Qt::PreciseTimer);
    connect(&live_timer_, &QTimer::timeout, this, &QtWorkbenchBridge::process_once);

    const QPointer<QtWorkbenchBridge> guarded{this};
    application_->set_background_wakeup([guarded] {
        if (guarded != nullptr) {
            QMetaObject::invokeMethod(guarded, &QtWorkbenchBridge::process_background_publication,
                                      Qt::QueuedConnection);
        }
    });
}

QtWorkbenchBridge::~QtWorkbenchBridge() { shutdown(); }

void QtWorkbenchBridge::shutdown() {
    if (shutting_down_) {
        return;
    }
    shutting_down_ = true;
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->cancel_background_work();
}

void QtWorkbenchBridge::enqueue(GuiCommand command) {
    if (shutting_down_ || !application_->enqueue(command)) {
        return;
    }
    schedule_immediate();
}

void QtWorkbenchBridge::run() { enqueue(GuiCommand::Run); }

void QtWorkbenchBridge::pause() { enqueue(GuiCommand::Pause); }

void QtWorkbenchBridge::reset() { enqueue(GuiCommand::Reset); }

void QtWorkbenchBridge::step() { enqueue(GuiCommand::StepNextEvent); }

void QtWorkbenchBridge::schedule_immediate() {
    if (fast_continuation_pending_ || shutting_down_) {
        return;
    }
    fast_continuation_pending_ = true;
    QTimer::singleShot(0, this, [this] {
        fast_continuation_pending_ = false;
        process_once();
    });
}

void QtWorkbenchBridge::schedule_for_run_state() {
    if (shutting_down_ || application_->run_state() != GuiRunState::Running) {
        live_timer_.stop();
        return;
    }
    if (application_->workspace().run_mode == GuiRunMode::Live) {
        if (!live_timer_.isActive()) {
            live_timer_.start();
        }
        return;
    }
    live_timer_.stop();
    schedule_immediate();
}

void QtWorkbenchBridge::emit_observable_changes(GuiRunState previous_state,
                                                const SimulationProgress& previous_progress,
                                                std::uint64_t previous_presentation_generation,
                                                const CompletedRunResult* previous_result) {
    const auto& current = application_->progress();
    if (application_->run_state() != previous_state) {
        Q_EMIT applicationStateChanged();
    }
    if (current.run_state != previous_progress.run_state ||
        current.current_tick != previous_progress.current_tick ||
        current.stop_tick != previous_progress.stop_tick ||
        current.event_count != previous_progress.event_count) {
        Q_EMIT progressChanged();
    }
    if (application_->presentation_generation() != previous_presentation_generation) {
        Q_EMIT presentationChanged(application_->presentation_generation());
    }
    if (application_->completed_result() != previous_result) {
        Q_EMIT completedResultChanged();
    }
}

void QtWorkbenchBridge::process_once() {
    if (shutting_down_) {
        return;
    }
    const auto previous_state = application_->run_state();
    const auto previous_progress = application_->progress();
    const auto previous_generation = application_->presentation_generation();
    const auto* previous_result = application_->completed_result();
    static_cast<void>(application_->update());
    emit_observable_changes(previous_state, previous_progress, previous_generation,
                            previous_result);
    schedule_for_run_state();
}

void QtWorkbenchBridge::process_background_publication() {
    if (shutting_down_) {
        return;
    }
    const auto* previous_result = application_->completed_result();
    if (application_->process_background_publications()) {
        if (application_->completed_result() != previous_result) {
            Q_EMIT completedResultChanged();
        }
        Q_EMIT statusChanged();
    }
}

void QtWorkbenchBridge::replace_session(std::unique_ptr<GuiSimulationSession> session) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->replace_session(std::move(session));
    Q_EMIT applicationStateChanged();
    Q_EMIT progressChanged();
    Q_EMIT presentationChanged(application_->presentation_generation());
    Q_EMIT completedResultChanged();
}

void QtWorkbenchBridge::emit_project_replaced() {
    Q_EMIT applicationStateChanged();
    Q_EMIT progressChanged();
    Q_EMIT presentationChanged(application_->presentation_generation());
    Q_EMIT completedResultChanged();
    Q_EMIT structuralSelectionChanged();
    Q_EMIT runtimeSelectionChanged();
    Q_EMIT draftChanged();
    Q_EMIT runConfigurationChanged();
    Q_EMIT workspaceChanged();
    Q_EMIT statusChanged();
}

void QtWorkbenchBridge::replace_project(std::unique_ptr<ProjectContext> project) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->replace_project(std::move(project));
    application_->record_active_project();
    emit_project_replaced();
}

void QtWorkbenchBridge::create_project(ProjectCreationRequest request,
                                       ProjectRuntimeInputs runtime_inputs) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->create_project(std::move(request), std::move(runtime_inputs));
    emit_project_replaced();
}

void QtWorkbenchBridge::open_project(const std::filesystem::path& project_file) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->open_project(project_file);
    emit_project_replaced();
}

void QtWorkbenchBridge::save_project() {
    application_->save_project();
    application_->set_status("Project saved.");
    Q_EMIT statusChanged();
}

bool QtWorkbenchBridge::apply_and_save_project() {
    auto& app = *application_;
    if (!app.has_active_project() || !app.editable_system().has_value()) {
        // No draft to apply; just save the project as-is.
        app.save_project();
        app.set_status("Project saved.");
        Q_EMIT statusChanged();
        return true;
    }
    const auto result = app.resolve_unapplied_changes(UnappliedSystemDecision::ApplyAndSave);
    if (result.status == ProjectTransitionStatus::Failed) {
        return false;
    }
    // After successful replacement, synchronize notifications.
    Q_EMIT applicationStateChanged();
    Q_EMIT draftChanged();
    Q_EMIT workspaceChanged();
    Q_EMIT structuralSelectionChanged();
    Q_EMIT statusChanged();
    return true;
}

void QtWorkbenchBridge::save_project_as(const std::filesystem::path& parent_directory,
                                        std::string new_name) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->save_project_as(parent_directory, std::move(new_name));
    emit_project_replaced();
}

void QtWorkbenchBridge::close_project() {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->close_project();
    emit_project_replaced();
}

void QtWorkbenchBridge::workspace_settings_changed() {
    normalize_workspace_state(application_->workspace());
    Q_EMIT workspaceChanged();
    schedule_for_run_state();
}

void QtWorkbenchBridge::notify_structural_selection_changed() {
    Q_EMIT structuralSelectionChanged();
}

void QtWorkbenchBridge::notify_runtime_selection_changed() { Q_EMIT runtimeSelectionChanged(); }

bool QtWorkbenchBridge::assign_task(TaskId task_id, std::optional<ResourceId> resource_id) {
    if (!application_->set_task_assignment(task_id, resource_id)) {
        Q_EMIT statusChanged();
        return false;
    }
    Q_EMIT draftChanged();
    Q_EMIT structuralSelectionChanged();
    Q_EMIT statusChanged();
    return true;
}

bool QtWorkbenchBridge::set_stop_tick(Tick stop_tick) {
    if (!application_->has_active_session() ||
        !application_->active_session().set_draft_stop_tick(stop_tick)) {
        application_->set_status("Pause the simulation before editing the stop tick.", true);
        Q_EMIT statusChanged();
        return false;
    }
    Q_EMIT runConfigurationChanged();
    return true;
}

bool QtWorkbenchBridge::load_run_plan(const std::filesystem::path& path) {
    if (!application_->has_active_session()) {
        return false;
    }
    const auto plan = cpssim::load_run_plan(path, application_->active_session().config());
    if (!application_->active_session().replace_draft(plan)) {
        application_->set_status("Pause the simulation before loading a run plan.", true);
        Q_EMIT statusChanged();
        return false;
    }
    application_->set_status("Run plan loaded into the unapplied draft.");
    Q_EMIT runConfigurationChanged();
    Q_EMIT statusChanged();
    return true;
}

bool QtWorkbenchBridge::save_run_plan(const std::filesystem::path& path) {
    if (!application_->has_active_session()) {
        return false;
    }
    const auto& validation = application_->active_session().validate_draft();
    if (!validation.plan.has_value() || !validation.diagnostics.empty()) {
        application_->set_status("Pending run plan is invalid.", true);
        Q_EMIT statusChanged();
        return false;
    }
    cpssim::save_run_plan(path, application_->active_session().config(), *validation.plan);
    application_->set_status("Run plan saved.");
    Q_EMIT statusChanged();
    return true;
}

bool QtWorkbenchBridge::validate_changes() {
    if (!application_->has_active_session()) {
        return false;
    }
    if (application_->has_active_project() && application_->editable_system().has_value()) {
        application_->validate_system_draft();
    } else {
        const auto& validation = application_->active_session().validate_draft();
        application_->set_status(validation.valid() ? "Run configuration is valid."
                                                    : validation.diagnostics.front().message,
                                 !validation.valid());
    }
    Q_EMIT statusChanged();
    Q_EMIT draftChanged();
    return !application_->status_is_error();
}

bool QtWorkbenchBridge::apply_and_restart() {
    if (!application_->has_active_session()) {
        return false;
    }
    auto applied = false;
    if (application_->has_active_project() && application_->editable_system().has_value()) {
        applied = application_->apply_system_draft();
    } else {
        applied = application_->active_session().apply_draft();
        if (applied) {
            application_->active_runtime_replaced();
            application_->set_status("Run configuration applied and restarted.");
        } else {
            application_->set_status("Run configuration could not be applied.", true);
        }
    }
    Q_EMIT applicationStateChanged();
    Q_EMIT presentationChanged(application_->presentation_generation());
    Q_EMIT progressChanged();
    Q_EMIT runConfigurationChanged();
    Q_EMIT draftChanged();
    Q_EMIT structuralSelectionChanged();
    Q_EMIT runtimeSelectionChanged();
    Q_EMIT statusChanged();
    return applied;
}

void QtWorkbenchBridge::restore_draft(EditableSystemDraft draft,
                                      std::vector<DraftTaskAssignment> assignments,
                                      StructuralSelection selection) {
    application_->restore_system_draft(std::move(draft), std::move(assignments),
                                       std::move(selection));
    Q_EMIT draftChanged();
    Q_EMIT structuralSelectionChanged();
    Q_EMIT statusChanged();
}

void QtWorkbenchBridge::set_resource_highlight(std::optional<ResourceId> resource_id) {
    if (resource_highlight_ == resource_id) {
        resource_highlight_.reset();
    } else {
        resource_highlight_ = resource_id;
    }
    Q_EMIT resourceHighlightChanged();
}

} // namespace cpssim::qt
