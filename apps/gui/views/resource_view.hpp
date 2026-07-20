/***
 * File: apps/gui/views/resource_view.hpp
 * Purpose: Declare rendering of detached resource state and utilization.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

namespace cpssim::gui {

/*** Draws resource rows and utilization derived only from a snapshot. ***/
struct ResourceViewState {
    bool restore_active_tab{true};
};

void draw_resource_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                        GuiResourceTab& active_tab, ResourceViewState& state);

} // namespace cpssim::gui
