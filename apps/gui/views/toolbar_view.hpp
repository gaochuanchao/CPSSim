/***
 * File: apps/gui/views/toolbar_view.hpp
 * Purpose: Declare the simulation command and status toolbar renderer.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/simulation_session.hpp"
#include "cpssim/gui/workspace_state.hpp"

namespace cpssim::gui {

/*** Draws controls that enqueue existing commands and copied run status. ***/
void draw_toolbar(GuiSimulationSession& session, const SimulationProgress& progress,
                  GuiWorkspaceState& workspace);

} // namespace cpssim::gui
