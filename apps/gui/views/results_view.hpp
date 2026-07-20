/*** Render shared generic metrics and optional Bosch result plots. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim::gui {

struct ResultsViewState {
    std::optional<RunResult> result;
    std::size_t event_count{};
    std::size_t observation_count{};
    Tick current_tick{-1};
    GuiRunState run_state{GuiRunState::NotConfigured};
    std::string scenario_kind;
};

void draw_results_view(const SimulationSnapshot& snapshot, std::string scenario_kind,
                       const std::vector<GuiSignalId>& selected_signals, GuiSelection& selection,
                       ResultsViewState& state);

} // namespace cpssim::gui
