/***
 * File: apps/gui/views/event_view.hpp
 * Purpose: Declare rendering of the detached canonical event log.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"

namespace cpssim::gui {

/*** Draws every canonical event in its existing JSON Lines representation. ***/
void draw_event_view(const SimulationSnapshot& snapshot, GuiSelection& selection);

} // namespace cpssim::gui
