/*** Derive reusable visualizer lanes without an ImPlot dependency. ***/

#include "cpssim/gui/plot_visualizer_model.hpp"

#include <algorithm>
#include <cctype>

namespace cpssim {

std::vector<const GuiSignalSeries*> search_plot_signals(const GuiSignalModel& model,
                                                        std::string_view query) {
    auto lower = [](std::string_view value) {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    };
    const auto needle = lower(query);
    std::vector<const GuiSignalSeries*> result;
    for (const auto& series : model.series) {
        const auto haystack = lower(series.descriptor.path + " " + series.descriptor.display_name);
        if (needle.empty() || haystack.find(needle) != std::string::npos)
            result.push_back(&series);
    }
    return result;
}

std::vector<GuiPlotLane> build_plot_lanes(const GuiSignalModel& model,
                                          const std::vector<GuiSignalId>& selected) {
    std::vector<GuiPlotLane> lanes;
    for (const auto& id : selected) {
        const auto* series = find_signal_series(model, id);
        if (series == nullptr)
            continue;
        const auto digital = id.scalar_type == GuiSignalScalarType::Boolean;
        const auto found = std::find_if(lanes.begin(), lanes.end(), [&](const auto& lane) {
            return lane.unit == series->descriptor.unit && lane.digital == digital;
        });
        if (found == lanes.end())
            lanes.push_back({series->descriptor.unit, digital, {series}});
        else
            found->series.push_back(series);
    }
    return lanes;
}

GuiPlotRange resolve_plot_range(const RunResult& result, GuiPlotRangeMode mode,
                                const std::optional<GuiTickRange>& selected, Tick custom_begin,
                                Tick custom_end) {
    if (mode == GuiPlotRangeMode::Selected && selected.has_value())
        return {std::max<Tick>(0, selected->begin_tick),
                std::min(result.snapshot.current_tick, selected->end_tick)};
    if (mode == GuiPlotRangeMode::Custom && custom_begin >= 0 && custom_end >= custom_begin)
        return {std::min(custom_begin, result.snapshot.current_tick),
                std::min(custom_end, result.snapshot.current_tick)};
    return {0, result.snapshot.current_tick};
}

double plot_tick_coordinate(Tick tick, GuiPlotXAxisUnit unit, PhysicalDuration tick_period) {
    return unit == GuiPlotXAxisUnit::Ticks
               ? static_cast<double>(tick)
               : static_cast<double>(tick) * static_cast<double>(tick_period.count()) / 1.0e9;
}

} // namespace cpssim
