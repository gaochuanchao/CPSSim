/*** Render read-only run results from the shared graphics-independent model. ***/

#include "results_view.hpp"

#include "cpssim/application/bosch_result_analysis.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace cpssim::gui {
namespace {

const char* unavailable = "Unavailable";

void update_result(const SimulationSnapshot& snapshot, const std::string& scenario_kind,
                   ResultsViewState& state) {
    if (!state.result.has_value() || state.event_count != snapshot.event_log.size() ||
        state.observation_count != snapshot.functional_observations.size() ||
        state.current_tick != snapshot.current_tick || state.run_state != snapshot.run_state ||
        state.scenario_kind != scenario_kind) {
        state.result = build_run_result(snapshot, scenario_kind);
        state.event_count = snapshot.event_log.size();
        state.observation_count = snapshot.functional_observations.size();
        state.current_tick = snapshot.current_tick;
        state.run_state = snapshot.run_state;
        state.scenario_kind = scenario_kind;
    }
}

void metric(const char* label, std::uint64_t value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%llu", static_cast<unsigned long long>(value));
}

void draw_summary(const RunMetrics& metrics) {
    if (ImGui::BeginTable("Run result summary", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch, 0.65F);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.35F);
        ImGui::TableHeadersRow();
        metric("Canonical events", metrics.event_count);
        metric("Completed jobs", metrics.completed_jobs);
        metric("Deadline misses", metrics.deadline_misses);
        metric("Preemptions", metrics.preemptions);
        metric("Messages sent", metrics.messages.sent);
        metric("Messages delivered", metrics.messages.delivered);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Simulation horizon");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%lld ticks", static_cast<long long>(metrics.horizon_tick));
        if (metrics.horizon_time.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%g s)",
                                static_cast<double>(metrics.horizon_time->count()) / 1.0e9);
        }
        ImGui::EndTable();
    }
}

void draw_response_times(const RunMetrics& metrics) {
    if (!ImGui::BeginTable("Response time results", 6,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    ImGui::TableSetupColumn("Task");
    ImGui::TableSetupColumn("Samples");
    ImGui::TableSetupColumn("Minimum (ticks)");
    ImGui::TableSetupColumn("Mean (ticks)");
    ImGui::TableSetupColumn("Maximum (ticks)");
    ImGui::TableSetupColumn("Total (ticks)");
    ImGui::TableHeadersRow();
    for (const auto& task : metrics.task_responses) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s (T%llu)", task.task_name.c_str(),
                    static_cast<unsigned long long>(task.task_id.value()));
        if (!task.response_time.has_value()) {
            for (int column = 1; column < 6; ++column) {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", unavailable);
            }
            continue;
        }
        const auto& value = *task.response_time;
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(value.count));
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(value.minimum));
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", value.mean());
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(value.maximum));
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(value.total));
    }
    ImGui::EndTable();
}

void draw_resources(const RunMetrics& metrics, GuiSelection& selection) {
    if (!ImGui::BeginTable("Result resource utilization", 5,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Resource");
    ImGui::TableSetupColumn("Busy ticks");
    ImGui::TableSetupColumn("Idle ticks");
    ImGui::TableSetupColumn("Utilization");
    ImGui::TableSetupColumn("Visual");
    ImGui::TableHeadersRow();
    for (const auto& resource : metrics.resources) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const auto selected = selection.resource_id() == resource.resource_id;
        const auto label = resource.resource_name + "##result-resource-" +
                           std::to_string(resource.resource_id.value());
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selection.select_resource(resource.resource_id);
        }
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(resource.busy_ticks));
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(resource.idle_ticks));
        ImGui::TableNextColumn();
        if (resource.utilization.has_value()) {
            ImGui::Text("%.2f%%", *resource.utilization * 100.0);
        } else {
            ImGui::TextDisabled("%s", unavailable);
        }
        ImGui::TableNextColumn();
        if (resource.utilization.has_value()) {
            ImGui::ProgressBar(static_cast<float>(*resource.utilization), ImVec2{-1.0F, 0.0F}, "");
        } else {
            ImGui::TextDisabled("No observed ticks");
        }
    }
    ImGui::EndTable();
}

std::optional<Tick> selected_tick(const RunResult& result, const GuiSelection& selection) {
    if (const auto range = selection.tick_range(); range.has_value() &&
                                                    range->begin_tick == range->end_tick) {
        return range->begin_tick;
    }
    if (const auto sequence = selection.event_sequence(); sequence.has_value()) {
        const auto found = std::find_if(result.snapshot.event_log.begin(),
                                        result.snapshot.event_log.end(), [&](const Event& event) {
                                            return event.sequence() == *sequence;
                                        });
        if (found != result.snapshot.event_log.end()) {
            return found->tick();
        }
    }
    return std::nullopt;
}

void draw_series_plot(const char* identity, const GuiSignalSeries& series, GuiSelection& selection,
                      const RunResult& result, const BoschResultAnalysis* bosch,
                      float requested_height) {
    auto size = ImVec2{ImGui::GetContentRegionAvail().x, requested_height};
    size.x = std::max(size.x, 120.0F);
    size.y = std::max(size.y, 90.0F);
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(identity, size, ImGuiButtonFlags_MouseButtonLeft);
    const auto left = origin.x + 54.0F;
    const auto right = origin.x + size.x - 8.0F;
    const auto top = origin.y + 18.0F;
    const auto bottom = origin.y + size.y - 24.0F;
    const auto width = std::max(right - left, 1.0F);
    const auto height = std::max(bottom - top, 1.0F);
    const auto end_tick = std::max<Tick>(result.snapshot.current_tick, 1);
    auto minimum = std::numeric_limits<double>::infinity();
    auto maximum = -std::numeric_limits<double>::infinity();
    for (const auto& sample : series.samples) {
        const auto value = gui_scalar_as_double(sample.value);
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    if (bosch != nullptr) {
        minimum = std::min(minimum, -0.2);
        maximum = std::max(maximum, 0.2);
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        minimum = 0.0;
        maximum = 1.0;
    } else if (minimum == maximum) {
        minimum -= 0.5;
        maximum += 0.5;
    }
    const auto tick_x = [&](Tick tick) {
        return left + static_cast<float>(static_cast<double>(tick) /
                                         static_cast<double>(end_tick)) * width;
    };
    const auto value_y = [&](double value) {
        return bottom - static_cast<float>((value - minimum) / (maximum - minimum)) * height;
    };
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                        ImGui::GetColorU32(ImGuiCol_WindowBg));
    draw->AddRect({left, top}, {right, bottom}, ImGui::GetColorU32(ImGuiCol_Border));
    if (bosch != nullptr) {
        for (const auto& interval : bosch->critical_intervals) {
            draw->AddRectFilled({tick_x(interval.begin_tick), top},
                                {tick_x(interval.end_tick + 1), bottom},
                                ImGui::GetColorU32(ImGuiCol_Header, 0.22F));
        }
        for (const auto threshold : {-0.2, 0.2}) {
            draw->AddLine({left, value_y(threshold)}, {right, value_y(threshold)},
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0F);
        }
        for (const auto tick : bosch->deadline_miss_ticks) {
            draw->AddTriangleFilled({tick_x(tick), top}, {tick_x(tick) - 4.0F, top + 8.0F},
                                    {tick_x(tick) + 4.0F, top + 8.0F},
                                    IM_COL32(240, 70, 70, 255));
        }
    }
    const auto downsampled = downsample_signal(
        series, {.begin_tick = 0,
                 .end_tick = end_tick,
                 .maximum_points =
                     std::max<std::size_t>(static_cast<std::size_t>(width) * 2, 4)});
    for (std::size_t index = 1; index < downsampled.size(); ++index) {
        draw->AddLine({tick_x(downsampled[index - 1].tick),
                       value_y(gui_scalar_as_double(downsampled[index - 1].value))},
                      {tick_x(downsampled[index].tick),
                       value_y(gui_scalar_as_double(downsampled[index].value))},
                      ImGui::GetColorU32(ImGuiCol_PlotLines), 1.8F);
    }
    if (const auto tick = selected_tick(result, selection); tick.has_value()) {
        draw->AddLine({tick_x(*tick), top}, {tick_x(*tick), bottom},
                      ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0F);
    }
    char maximum_label[48]{};
    char minimum_label[48]{};
    std::snprintf(maximum_label, sizeof(maximum_label), "%.5g", maximum);
    std::snprintf(minimum_label, sizeof(minimum_label), "%.5g", minimum);
    draw->AddText({origin.x + 3.0F, top}, ImGui::GetColorU32(ImGuiCol_TextDisabled), maximum_label);
    draw->AddText({origin.x + 3.0F, bottom - ImGui::GetFontSize()},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), minimum_label);
    draw->AddText({left + 5.0F, top + 3.0F}, ImGui::GetColorU32(ImGuiCol_Text),
                  series.descriptor.display_name.c_str());
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto mouse_x = std::clamp(ImGui::GetIO().MousePos.x, left, right);
        const auto tick = static_cast<Tick>(std::llround(
            static_cast<double>(mouse_x - left) / static_cast<double>(width) *
            static_cast<double>(end_tick)));
        selection.select_tick(tick);
    }
}

void draw_bosch_results(const RunResult& result, const std::vector<GuiSignalId>& selected_signals,
                        GuiSelection& selection) {
    const auto bosch = derive_bosch_result_analysis(result);
    ImGui::SeparatorText("Bosch control results");
    if (bosch.lateral_error == nullptr) {
        ImGui::TextDisabled("%s", bosch.diagnostic.value_or("Lateral error is unavailable.").c_str());
        return;
    }
    ImGui::TextDisabled("Blue band: critical section | dashed bounds: +/-0.2 m | red marker: deadline miss");
    draw_series_plot("Bosch lateral result plot", *bosch.lateral_error, selection, result, &bosch,
                     230.0F);
    for (const auto& id : selected_signals) {
        if (id == bosch_lateral_error_signal_id() || id == bosch_critical_section_signal_id()) {
            continue;
        }
        if (const auto* series = find_signal_series(*result.signals.model, id); series != nullptr) {
            ImGui::Spacing();
            ImGui::PushID(series->descriptor.path.c_str());
            draw_series_plot("Selected Bosch signal", *series, selection, result, nullptr, 145.0F);
            ImGui::PopID();
        }
    }
}

} // namespace

void draw_results_view(const SimulationSnapshot& snapshot, std::string scenario_kind,
                       const std::vector<GuiSignalId>& selected_signals,
                       GuiSelection& selection, ResultsViewState& state) {
    update_result(snapshot, scenario_kind, state);
    const auto& result = *state.result;
    draw_summary(result.metrics);
    ImGui::Spacing();
    ImGui::SeparatorText("Resource utilization");
    draw_resources(result.metrics, selection);
    ImGui::Spacing();
    ImGui::SeparatorText("Task response times");
    draw_response_times(result.metrics);
    ImGui::Spacing();
    ImGui::SeparatorText("Message timing");
    if (result.metrics.messages.delivery_delay.has_value()) {
        const auto& delay = *result.metrics.messages.delivery_delay;
        ImGui::Text("%llu paired deliveries | min %lld | mean %.3f | max %lld ticks",
                    static_cast<unsigned long long>(delay.count),
                    static_cast<long long>(delay.minimum), delay.mean(),
                    static_cast<long long>(delay.maximum));
    } else {
        ImGui::TextDisabled("Message delivery delay: %s", unavailable);
    }
    if (result.scenario_kind == "bosch") {
        draw_bosch_results(result, selected_signals, selection);
    }
}

} // namespace cpssim::gui
