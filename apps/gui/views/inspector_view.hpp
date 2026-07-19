/***
 * File: apps/gui/views/inspector_view.hpp
 * Purpose: Declare read-only inspection of the shared stable-ID selection.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"

namespace cpssim::gui {

/*** Draws immutable and copied runtime details for the selected entity. ***/
void draw_inspector_view(const SimulationSnapshot& snapshot, GuiSelection& selection);

} // namespace cpssim::gui
