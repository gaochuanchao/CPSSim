/***
 * File: apps/gui/views/run_plan_editor.hpp
 * Purpose: Declare explicit run-plan draft controls and diagnostics rendering.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/simulation_session.hpp"

namespace cpssim::gui {

/*** Draws active/draft plan distinction, typed fields, and Apply controls. ***/
void draw_run_plan_editor(GuiSimulationSession& session, const SimulationSnapshot& snapshot);

} // namespace cpssim::gui
