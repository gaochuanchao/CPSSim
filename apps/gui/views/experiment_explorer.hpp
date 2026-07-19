/***
 * File: apps/gui/views/experiment_explorer.hpp
 * Purpose: Declare the read-only validated experiment tree renderer.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/presentation_model.hpp"
#include "cpssim/gui/selection_model.hpp"

namespace cpssim::gui {

/*** Draws the complete experiment hierarchy and updates shared selection. ***/
void draw_experiment_explorer(const ExperimentPresentationSnapshot& experiment,
                              GuiSelection& selection);

} // namespace cpssim::gui
