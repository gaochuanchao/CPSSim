/***
 * File: apps/gui/views/signal_view.hpp
 * Purpose: Declare the G06 custom scalar-signal plot and view state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/signal_series.hpp"

#include <optional>
#include <vector>

namespace cpssim::gui {

/*** Owns disposable plot selection, viewport, and incremental series state. ***/
struct SignalViewState {
    GuiSignalCache cache;
    std::vector<GuiSignalId> selected_signals;
    double view_begin_tick{0.0};
    double view_end_tick{1.0};
    bool fit_requested{true};
    bool auto_follow{true};
    bool selection_initialized{false};
    std::optional<Tick> drag_begin_tick;
};

/*** Draws functional signal discovery and synchronized scalar plots. ***/
void draw_signal_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                      SignalViewState& state);

} // namespace cpssim::gui
