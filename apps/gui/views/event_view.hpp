/***
 * File: apps/gui/views/event_view.hpp
 * Purpose: Declare rendering of the detached canonical event log.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/event_table_model.hpp"
#include "cpssim/gui/gui_profiler.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <array>

namespace cpssim::gui {

/*** Draws every canonical event in its existing JSON Lines representation. ***/
struct EventViewState {
    std::array<char, 256> text_filter{};
    bool filter_initialized{false};
    GuiEventTableCache cache;
};

void draw_event_view(const SimulationSnapshot& snapshot, std::uint64_t presentation_generation,
                     GuiSelection& selection, GuiEventFilters& filters,
                     GuiEventColumnVisibility& columns, EventViewState& state,
                     GuiProfiler* profiler = nullptr);

} // namespace cpssim::gui
