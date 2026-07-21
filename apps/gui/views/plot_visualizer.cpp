/*** Render full-resolution completed data through a CPSSim-owned ImPlot state. ***/

#include "plot_visualizer.hpp"

#include "cpssim/application/bosch_result_analysis.hpp"
#include "cpssim/gui/plot_visualizer_model.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cpssim::gui {
namespace {

bool selected(const std::vector<GuiSignalId>& values, const GuiSignalId& id) {
    return std::find(values.begin(), values.end(), id) != values.end();
}

void signal_browser(const GuiSignalModel& model, GuiWorkspaceState& workspace,
                    PlotVisualizerViewState& state) {
    ImGui::InputTextWithHint("##plot-search", "Search signals...", state.search.data(),
                             state.search.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
        workspace.selected_signals.clear();
    ImGui::BeginChild("Signal browser", ImVec2{15.0F * ImGui::GetFontSize(), 150.0F},
                      ImGuiChildFlags_Borders);
    for (const auto* series : search_plot_signals(model, state.search.data())) {
        auto enabled = selected(workspace.selected_signals, series->descriptor.id);
        if (ImGui::Checkbox(series->descriptor.path.c_str(), &enabled)) {
            if (enabled)
                workspace.selected_signals.push_back(series->descriptor.id);
            else
                std::erase(workspace.selected_signals, series->descriptor.id);
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Selected plot signals", ImVec2{0.0F, 150.0F}, ImGuiChildFlags_Borders);
    for (std::size_t index = 0; index < workspace.selected_signals.size(); ++index) {
        const auto* series = find_signal_series(model, workspace.selected_signals[index]);
        if (series == nullptr)
            continue;
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextUnformatted(series->descriptor.display_name.c_str());
        ImGui::SameLine();
        if (index > 0 && ImGui::SmallButton("Up"))
            std::swap(workspace.selected_signals[index], workspace.selected_signals[index - 1]);
        ImGui::SameLine();
        if (index + 1 < workspace.selected_signals.size() && ImGui::SmallButton("Down"))
            std::swap(workspace.selected_signals[index], workspace.selected_signals[index + 1]);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void controls(GuiWorkspaceState& workspace, PlotVisualizerViewState& state, bool bosch) {
    int axis = workspace.plot_x_axis_unit == GuiPlotXAxisUnit::Seconds ? 1 : 0;
    if (ImGui::Combo("X axis", &axis, "Ticks\0Seconds\0"))
        workspace.plot_x_axis_unit =
            axis == 1 ? GuiPlotXAxisUnit::Seconds : GuiPlotXAxisUnit::Ticks;
    ImGui::SameLine();
    int range = static_cast<int>(workspace.plot_range_mode);
    if (ImGui::Combo("Range", &range, "Full run\0Selected\0Custom\0"))
        workspace.plot_range_mode = static_cast<GuiPlotRangeMode>(range);
    if (workspace.plot_range_mode == GuiPlotRangeMode::Custom) {
        ImGui::SetNextItemWidth(120.0F);
        ImGui::InputScalar("Begin tick", ImGuiDataType_S64, &workspace.plot_custom_begin);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0F);
        ImGui::InputScalar("End tick", ImGuiDataType_S64, &workspace.plot_custom_end);
    }
    ImGui::Checkbox("Automatic Y", &workspace.plot_auto_y);
    if (!workspace.plot_auto_y) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0F);
        ImGui::InputDouble("Y minimum", &workspace.plot_y_min);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0F);
        ImGui::InputDouble("Y maximum", &workspace.plot_y_max);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &workspace.plot_grid);
    ImGui::SameLine();
    ImGui::Checkbox("Legend", &workspace.plot_legend);
    ImGui::SameLine();
    ImGui::Checkbox("Markers", &workspace.plot_markers);
    ImGui::SliderFloat("Thickness", &workspace.plot_line_thickness, 0.5F, 8.0F, "%.1f");
    if (ImGui::Button("Auto-fit"))
        state.fit_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) {
        workspace.plot_range_mode = GuiPlotRangeMode::Full;
        workspace.plot_auto_y = true;
        state.fit_requested = true;
    }
    if (bosch) {
        ImGui::SeparatorText("Bosch overlays");
        ImGui::Checkbox("±0.2 m thresholds", &workspace.plot_bosch_thresholds);
        ImGui::SameLine();
        ImGui::Checkbox("Critical sections", &workspace.plot_bosch_critical_sections);
        ImGui::SameLine();
        ImGui::Checkbox("Deadline misses", &workspace.plot_bosch_deadline_misses);
        ImGui::SameLine();
        ImGui::Checkbox("Selected tick", &workspace.plot_selected_tick);
    }
}

const GuiScalarSample* nearest_sample(const GuiSignalSeries& series, Tick tick) {
    const auto found =
        std::lower_bound(series.samples.begin(), series.samples.end(), tick,
                         [](const auto& sample, Tick value) { return sample.tick < value; });
    if (found == series.samples.begin())
        return found == series.samples.end() ? nullptr : &*found;
    if (found == series.samples.end())
        return &series.samples.back();
    const auto previous = std::prev(found);
    return tick - previous->tick <= found->tick - tick ? &*previous : &*found;
}

void draw_lane(const RunResult& result, const GuiPlotLane& lane, GuiWorkspaceState& workspace,
               GuiSelection& selection, const BoschResultAnalysis* bosch, bool fit_requested) {
    const auto range = resolve_plot_range(result, workspace.plot_range_mode, selection.tick_range(),
                                          workspace.plot_custom_begin, workspace.plot_custom_end);
    const auto x_min =
        plot_tick_coordinate(range.begin, workspace.plot_x_axis_unit, result.metrics.tick_period);
    const auto x_max = plot_tick_coordinate(std::max(range.end, range.begin + 1),
                                            workspace.plot_x_axis_unit, result.metrics.tick_period);
    ImPlot::SetNextAxisLimits(ImAxis_X1, x_min, x_max,
                              workspace.plot_range_mode == GuiPlotRangeMode::Custom
                                  ? ImGuiCond_Always
                                  : ImGuiCond_Appearing);
    if (!workspace.plot_auto_y && workspace.plot_y_max > workspace.plot_y_min)
        ImPlot::SetNextAxisLimits(ImAxis_Y1, workspace.plot_y_min, workspace.plot_y_max,
                                  ImGuiCond_Always);
    if (fit_requested)
        ImPlot::SetNextAxesToFit();
    auto flags = workspace.plot_legend ? ImPlotFlags_None : ImPlotFlags_NoLegend;
    if (ImPlot::BeginPlot((lane.unit.empty() ? "Values" : lane.unit).c_str(), ImVec2{-1.0F, 220.0F},
                          flags)) {
        ImPlotAxisFlags axis_flags =
            workspace.plot_grid ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoGridLines;
        ImPlot::SetupAxes(workspace.plot_x_axis_unit == GuiPlotXAxisUnit::Seconds ? "Seconds"
                                                                                  : "Ticks",
                          lane.unit.c_str(), axis_flags, axis_flags);
        for (const auto* series : lane.series) {
            const auto samples = downsample_signal(*series, {range.begin, range.end, 4000});
            std::vector<double> x;
            std::vector<double> y;
            x.reserve(samples.size());
            y.reserve(samples.size());
            for (const auto& sample : samples) {
                x.push_back(plot_tick_coordinate(sample.tick, workspace.plot_x_axis_unit,
                                                 result.metrics.tick_period));
                y.push_back(gui_scalar_as_double(sample.value));
            }
            ImPlotSpec spec;
            spec.LineWeight = workspace.plot_line_thickness;
            if (workspace.plot_markers) {
                spec.Marker = ImPlotMarker_Circle;
                spec.MarkerSize = 2.0F;
            }
            if (lane.digital)
                ImPlot::PlotStairs(series->descriptor.display_name.c_str(), x.data(), y.data(),
                                   static_cast<int>(x.size()), spec);
            else
                ImPlot::PlotLine(series->descriptor.display_name.c_str(), x.data(), y.data(),
                                 static_cast<int>(x.size()), spec);
        }
        if (bosch != nullptr && workspace.plot_bosch_thresholds && lane.unit == "m") {
            const double xs[]{x_min, x_max};
            const double upper[]{0.2, 0.2};
            const double lower[]{-0.2, -0.2};
            ImPlot::PlotLine("+0.2 m", xs, upper, 2);
            ImPlot::PlotLine("-0.2 m", xs, lower, 2);
        }
        if (bosch != nullptr && workspace.plot_bosch_critical_sections) {
            for (std::size_t index = 0; index < bosch->critical_intervals.size(); ++index) {
                const auto& interval = bosch->critical_intervals[index];
                const double xs[]{
                    plot_tick_coordinate(interval.begin_tick, workspace.plot_x_axis_unit,
                                         result.metrics.tick_period),
                    plot_tick_coordinate(interval.end_tick + 1, workspace.plot_x_axis_unit,
                                         result.metrics.tick_period)};
                const double low[]{-1.0e12, -1.0e12};
                const double high[]{1.0e12, 1.0e12};
                ImPlotSpec shade;
                shade.FillAlpha = 0.12F;
                shade.Flags = ImPlotItemFlags_NoFit;
                const auto shade_label = "Critical section##" + std::to_string(index);
                ImPlot::PlotShaded(shade_label.c_str(), xs, low, high, 2, shade);
            }
        }
        if (bosch != nullptr && workspace.plot_bosch_deadline_misses &&
            !bosch->deadline_miss_ticks.empty()) {
            std::vector<double> ticks;
            for (const auto tick : bosch->deadline_miss_ticks)
                ticks.push_back(plot_tick_coordinate(tick, workspace.plot_x_axis_unit,
                                                     result.metrics.tick_period));
            ImPlot::PlotInfLines("Deadline misses", ticks.data(), static_cast<int>(ticks.size()));
        }
        if (workspace.plot_selected_tick)
            if (const auto tick = selection.tick_range();
                tick.has_value() && tick->begin_tick == tick->end_tick) {
                const double x = plot_tick_coordinate(tick->begin_tick, workspace.plot_x_axis_unit,
                                                      result.metrics.tick_period);
                ImPlot::PlotInfLines("Selected tick", &x, 1);
            }
        if (ImPlot::IsPlotHovered()) {
            const auto mouse = ImPlot::GetPlotMousePos();
            const auto tick = workspace.plot_x_axis_unit == GuiPlotXAxisUnit::Ticks
                                  ? static_cast<Tick>(std::llround(mouse.x))
                                  : static_cast<Tick>(std::llround(
                                        mouse.x * 1.0e9 /
                                        static_cast<double>(result.metrics.tick_period.count())));
            ImGui::BeginTooltip();
            ImGui::Text("Nearest tick: %lld", static_cast<long long>(tick));
            for (const auto* series : lane.series) {
                if (const auto* sample = nearest_sample(*series, tick); sample != nullptr) {
                    ImGui::Text("%s: %.9g %s", series->descriptor.display_name.c_str(),
                                gui_scalar_as_double(sample->value),
                                series->descriptor.unit.c_str());
                }
            }
            ImGui::EndTooltip();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                selection.select_tick(std::clamp<Tick>(tick, 0, result.snapshot.current_tick));
        }
        ImPlot::EndPlot();
    }
}

} // namespace

void draw_plot_visualizer(bool& open, const CompletedRunResult* completed,
                          GuiWorkspaceState& workspace, GuiSelection& selection,
                          PlotVisualizerViewState& state, GuiPointerRegionMap* pointer_regions) {
    if (!open)
        return;
    if (!ImGui::Begin("Plot Visualizer", &open)) {
        ImGui::End();
        return;
    }
    if (pointer_regions != nullptr) {
        const auto position = ImGui::GetWindowPos();
        pointer_regions->add(
            {ImGui::GetID("Plot visualizer canvas"),
             {position.x, position.y, position.x + ImGui::GetWindowWidth(),
              position.y + ImGui::GetWindowHeight()},
             GuiPointerRegionBehavior::PositionSensitive});
    }
    if (completed == nullptr || !completed->result->signals.model.has_value()) {
        ImGui::TextDisabled("No completed run is available.");
        ImGui::End();
        return;
    }
    const auto& result = *completed->result;
    if (!state.initialized) {
        if (result.scenario_kind == "bosch" && workspace.selected_signals.empty()) {
            workspace.selected_signals.push_back(bosch_lateral_error_signal_id());
            workspace.plot_x_axis_unit = GuiPlotXAxisUnit::Seconds;
        }
        state.initialized = true;
    }
    signal_browser(*result.signals.model, workspace, state);
    const auto is_bosch = result.scenario_kind == "bosch";
    controls(workspace, state, is_bosch);
    const auto lanes = build_plot_lanes(*result.signals.model, workspace.selected_signals);
    const auto bosch = result.scenario_kind == "bosch"
                           ? std::optional{derive_bosch_result_analysis(result)}
                           : std::nullopt;
    for (const auto& lane : lanes)
        draw_lane(result, lane, workspace, selection, bosch ? &*bosch : nullptr,
                  state.fit_requested);
    state.fit_requested = false;
    if (lanes.empty())
        ImGui::TextDisabled("Select one or more signals to plot.");
    ImGui::End();
}

} // namespace cpssim::gui
