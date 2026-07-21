/*** Graphics-independent completed-run plot browsing and range projection. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <string_view>
#include <optional>

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

std::size_t plot_point_budget(float logical_width) noexcept;

struct GuiPlotSeriesProjection {
    GuiSignalId id;
    std::vector<GuiScalarSample> samples;
};

class GuiPlotDataCache {
  public:
    bool update(std::uint64_t run_generation, const GuiSignalModel& model,
                const std::vector<GuiSignalId>& selected, GuiPlotXAxisUnit axis_unit,
                GuiPlotRange range, float logical_width);
    const std::vector<GuiPlotSeriesProjection>& series() const noexcept { return series_; }
    const GuiPlotSeriesProjection* find(const GuiSignalId& id) const noexcept;
    std::uint64_t build_count() const noexcept { return build_count_; }
  private:
    std::uint64_t run_generation_{};
    std::vector<GuiSignalId> selected_;
    GuiPlotXAxisUnit axis_unit_{GuiPlotXAxisUnit::Ticks};
    GuiPlotRange range_{};
    std::size_t point_budget_{};
    bool initialized_{false};
    std::vector<GuiPlotSeriesProjection> series_;
    std::uint64_t build_count_{};
};

class GuiPlotSignalSearchCache {
  public:
    const std::vector<const GuiSignalSeries*>& update(std::uint64_t run_generation,
                                                       const GuiSignalModel& model,
                                                       std::string_view query);
    std::uint64_t build_count() const noexcept { return build_count_; }
  private:
    std::uint64_t run_generation_{};
    std::string query_;
    bool initialized_{false};
    std::vector<const GuiSignalSeries*> matches_;
    std::uint64_t build_count_{};
};

} // namespace cpssim
