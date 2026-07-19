/***
 * File: apps/gui/views/timeline_view.hpp
 * Purpose: Declare the G05 scheduling-timeline canvas and presentation state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/timeline_model.hpp"

namespace cpssim::gui {

/*** Owns disposable viewport, visibility, and incremental derivation state. ***/
struct TimelineViewState {
    GuiTimelineCache cache;
    double view_begin_tick{0.0};
    double pixels_per_tick{1.0};
    bool fit_requested{true};
    bool show_ready{true};
    bool show_running{true};
    bool show_markers{true};
    bool filter_to_selection{false};
};

/*** Draws the live timeline and reports whether a double-click requests Inspector. ***/
bool draw_timeline_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                        TimelineViewState& state);

} // namespace cpssim::gui
