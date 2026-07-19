/***
 * File: apps/gui/views/toolbar_view.hpp
 * Purpose: Declare the simulation command and status toolbar renderer.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/simulation_session.hpp"

namespace cpssim::gui {

/*** Draws controls that enqueue existing commands and copied run status. ***/
void draw_toolbar(GuiSimulationSession& session, const SimulationSnapshot& snapshot);

} // namespace cpssim::gui
