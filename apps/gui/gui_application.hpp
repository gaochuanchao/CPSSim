/***
 * File: apps/gui/gui_application.hpp
 * Purpose: Declare the graphics-only CPSSim workbench shell and its
 *          presentation-owned panel state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The application coordinates views over detached snapshots and sends
 *        commands through SimulationController. It owns no kernel state.
 ***/

#pragma once

#include "views/architecture_view.hpp"
#include "views/signal_view.hpp"
#include "views/timeline_view.hpp"

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/simulation_session.hpp"

#include <array>
#include <string>

namespace cpssim::gui {

/*** Owns workbench presentation state and draws one immediate-mode frame. ***/
class GuiApplication {
  public:
    explicit GuiApplication(GuiSimulationSession& session);

    // Draws the workbench from one detached point-in-time simulation copy.
    void draw_frame(const SimulationSnapshot& snapshot);

  private:
    enum class RunPlanFileAction {
        Load,
        Save,
    };

    // Draws menus that affect presentation state but never kernel state.
    void draw_main_menu();

    // Draws the modal application information requested through Help.
    void draw_about_dialog();

    // Draws shared JSON run-plan Load/Save path entry and located errors.
    void draw_run_plan_file_dialog();

    // Draws the existing resource/event stack in the center workbench column.
    void draw_center_panels(const SimulationSnapshot& snapshot);

    GuiSimulationSession& session_;
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
    bool run_plan_file_error_{false};
    ArchitectureViewState architecture_view_state_;
    TimelineViewState timeline_view_state_;
    SignalViewState signal_view_state_;
    float text_scale_{1.0F};
};

} // namespace cpssim::gui
