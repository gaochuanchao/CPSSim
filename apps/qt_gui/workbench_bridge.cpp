/*** Drive CPSSim cooperatively from the Qt GUI thread. ***/
#include "apps/qt_gui/workbench_bridge.hpp"

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

void QtWorkbenchBridge::open_project(const std::filesystem::path& project_file) {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->open_project(project_file);
    Q_EMIT applicationStateChanged();
    Q_EMIT progressChanged();
    Q_EMIT presentationChanged(application_->presentation_generation());
    Q_EMIT completedResultChanged();
}

void QtWorkbenchBridge::close_project() {
    live_timer_.stop();
    fast_continuation_pending_ = false;
    application_->close_project();
    Q_EMIT applicationStateChanged();
    Q_EMIT progressChanged();
    Q_EMIT presentationChanged(application_->presentation_generation());
    Q_EMIT completedResultChanged();
}

void QtWorkbenchBridge::notify_structural_selection_changed() {
    Q_EMIT structuralSelectionChanged();
}

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
