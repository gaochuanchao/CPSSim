/***
 * File: apps/gui/views/event_view.hpp
 * Purpose: Declare rendering of the detached canonical event log.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <array>

namespace cpssim::gui {

/*** Draws every canonical event in its existing JSON Lines representation. ***/
struct EventViewState {
    std::array<char, 256> text_filter{};
    bool filter_initialized{false};
};

void draw_event_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                     GuiEventFilters& filters, GuiEventColumnVisibility& columns,
                     EventViewState& state);

} // namespace cpssim::gui
