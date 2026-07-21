/*** Declare the non-modal completed-run ImPlot visualizer. ***/

#pragma once

#include "cpssim/analysis/completed_run_result.hpp"
#include "cpssim/gui/frame_scheduler.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <array>

namespace cpssim::gui {

struct PlotVisualizerViewState {
    std::array<char, 128> search{};
    bool initialized{false};
    bool fit_requested{false};
};

void draw_plot_visualizer(bool& open, const CompletedRunResult* completed,
                          GuiWorkspaceState& workspace, GuiSelection& selection,
                          PlotVisualizerViewState& state,
                          GuiPointerRegionMap* pointer_regions = nullptr);

} // namespace cpssim::gui
