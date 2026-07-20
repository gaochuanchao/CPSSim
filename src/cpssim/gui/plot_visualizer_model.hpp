/*** Graphics-independent completed-run plot browsing and range projection. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/gui/workspace_state.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <string_view>

namespace cpssim {

struct GuiPlotLane {
    std::string unit;
    bool digital{false};
    std::vector<const GuiSignalSeries*> series;
};

struct GuiPlotRange {
    Tick begin{};
    Tick end{};
    bool operator==(const GuiPlotRange&) const = default;
};

std::vector<const GuiSignalSeries*> search_plot_signals(const GuiSignalModel& model,
                                                        std::string_view query);
std::vector<GuiPlotLane> build_plot_lanes(const GuiSignalModel& model,
                                          const std::vector<GuiSignalId>& selected);
GuiPlotRange resolve_plot_range(const RunResult& result, GuiPlotRangeMode mode,
                                const std::optional<GuiTickRange>& selected, Tick custom_begin,
                                Tick custom_end);
double plot_tick_coordinate(Tick tick, GuiPlotXAxisUnit unit, PhysicalDuration tick_period);

} // namespace cpssim
