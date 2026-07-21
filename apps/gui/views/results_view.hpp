/*** Render shared generic metrics and optional Bosch result plots. ***/

#pragma once

#include "cpssim/analysis/completed_run_result.hpp"
#include "cpssim/analysis/completed_run_finalizer.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/frame_scheduler.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim::gui {

struct ResultsViewState {};

void draw_results_view(const SimulationProgress& progress, const CompletedRunResult* completed,
                       CompletedResultFinalizationState finalization_state,
                       bool& open_visualizer, bool& open_export, GuiWorkspaceState& workspace,
                       ResultsViewState& state, GuiPointerRegionMap* pointer_regions = nullptr);

} // namespace cpssim::gui
