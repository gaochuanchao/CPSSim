/*** Qt event-loop adapter around the graphics-independent workbench owner. ***/
#pragma once

#ifndef Q_MOC_RUN
#include "cpssim/application/workbench_application.hpp"
#else
namespace cpssim {
class CompletedRunResult;
class GuiSimulationSession;
class WorkbenchApplication;
enum class GuiCommand;
enum class GuiRunState;
struct SimulationProgress;
} // namespace cpssim
#endif

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace cpssim::qt {

class QtWorkbenchBridge final : public QObject {
    Q_OBJECT

  public:
    explicit QtWorkbenchBridge(std::unique_ptr<WorkbenchApplication> application,
                               QObject* parent = nullptr);
    ~QtWorkbenchBridge() override;

    WorkbenchApplication& application() noexcept { return *application_; }
    const WorkbenchApplication& application() const noexcept { return *application_; }
    GuiRunState run_state() const noexcept { return application_->run_state(); }
    const SimulationProgress& progress() const noexcept { return application_->progress(); }
    bool live_timer_active() const noexcept { return live_timer_.isActive(); }
    bool fast_continuation_pending() const noexcept { return fast_continuation_pending_; }

    void replace_session(std::unique_ptr<GuiSimulationSession> session);
    void open_project(const std::filesystem::path& project_file);
    void close_project();
    void shutdown();
    void notify_structural_selection_changed();
    bool assign_task(TaskId task_id, std::optional<ResourceId> resource_id);
    void set_resource_highlight(std::optional<ResourceId> resource_id);
    std::optional<ResourceId> resource_highlight() const noexcept { return resource_highlight_; }

  public Q_SLOTS:
    void run();
    void pause();
    void reset();
    void step();
    void process_once();
    void process_background_publication();

  Q_SIGNALS:
    void applicationStateChanged();
    void progressChanged();
    void presentationChanged(quint64 generation);
    void completedResultChanged();
    void statusChanged();
    void structuralSelectionChanged();
    void draftChanged();
    void resourceHighlightChanged();

  private:
    void enqueue(GuiCommand command);
    void schedule_immediate();
    void schedule_for_run_state();
    void emit_observable_changes(GuiRunState previous_state,
                                 const SimulationProgress& previous_progress,
                                 std::uint64_t previous_presentation_generation,
                                 const CompletedRunResult* previous_result);

    std::unique_ptr<WorkbenchApplication> application_;
    QTimer live_timer_;
    bool fast_continuation_pending_{false};
    bool shutting_down_{false};
    std::optional<ResourceId> resource_highlight_;
};

} // namespace cpssim::qt
