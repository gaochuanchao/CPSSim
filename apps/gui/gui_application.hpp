/***
 * File: apps/gui/gui_application.hpp
 * Purpose: Declare the graphics-only CPSSim home/workbench shell and its
 *          optional session and presentation-owned panel state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The application owns an optional GuiSimulationSession, coordinates
 *        views over detached snapshots, and sends commands through the
 *        session boundary. It never accesses mutable kernel containers.
 ***/

#pragma once

#include "views/architecture_view.hpp"
#include "views/signal_view.hpp"
#include "views/timeline_view.hpp"

#include "cpssim/gui/application_state.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <array>
#include <memory>
#include <string>

namespace cpssim::gui {

/*** Owns optional session and presentation state and draws one GUI frame. ***/
class GuiApplication {
  public:
    GuiApplication();
    explicit GuiApplication(std::unique_ptr<GuiSimulationSession> session);

    GuiApplicationScreen screen() const noexcept { return application_state_.screen(); }
    bool has_active_session() const noexcept { return application_state_.has_active_session(); }
    void replace_session(std::unique_ptr<GuiSimulationSession> session);
    void clear_session();
    void update_active_session();

    // Draws Home or a workbench backed by one detached point-in-time copy.
    void draw_frame();

  private:
    enum class RunPlanFileAction {
        Load,
        Save,
    };

    // Draws menus that affect presentation state but never kernel state.
    void draw_main_menu();

    // Draws startup actions without constructing a simulation session.
    void draw_home_screen();

    // Draws the active workbench from a detached session snapshot.
    void draw_workbench(const SimulationSnapshot& snapshot);

    // Draws the modal application information requested through Help.
    void draw_about_dialog();

    // Draws shared JSON run-plan Load/Save path entry and located errors.
    void draw_run_plan_file_dialog();

    // Draws the existing resource/event stack in the center workbench column.
    void draw_center_panels(const SimulationSnapshot& snapshot);

    void initialize_presentation_state();

    GuiApplicationState application_state_;
    GuiSelection selection_;
    bool show_explorer_{true};
    bool show_inspector_{true};
    bool show_architecture_{true};
    bool show_timeline_{true};
    bool show_signals_{true};
    bool show_resources_{true};
    bool show_events_{true};
    bool open_about_{false};
    bool open_run_plan_file_dialog_{false};
    RunPlanFileAction run_plan_file_action_{RunPlanFileAction::Load};
    std::array<char, 1024> run_plan_path_{};
    std::string run_plan_file_status_;
    std::string home_action_status_;
    bool run_plan_file_error_{false};
    ArchitectureViewState architecture_view_state_;
    TimelineViewState timeline_view_state_;
    SignalViewState signal_view_state_;
    float text_scale_{1.0F};
};

} // namespace cpssim::gui
