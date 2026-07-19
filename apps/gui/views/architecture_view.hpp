/***
 * File: apps/gui/views/architecture_view.hpp
 * Purpose: Declare G04 draw-list rendering and workspace interaction state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/architecture_graph.hpp"
#include "cpssim/gui/simulation_session.hpp"

#include <optional>
#include <string>

namespace cpssim::gui {

/*** Owns disposable workspace state; no value affects simulation behavior. ***/
struct ArchitectureViewState {
    float pan_x{0.0F};
    float pan_y{0.0F};
    float zoom{1.0F};
    bool fit_requested{true};
    std::optional<TaskId> pressed_task;
    bool dragging_task{false};
    std::string status;
    bool status_error{false};
};

/***
 * Draws one graph canvas and returns true when a double-click requests the
 * Inspector panel.
 ***/
bool draw_architecture_view(const GuiArchitectureGraph& graph, GuiSimulationSession& session,
                            const SimulationSnapshot& snapshot, GuiSelection& selection,
                            ArchitectureViewState& state);

} // namespace cpssim::gui
