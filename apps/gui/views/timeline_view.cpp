/***
 * File: apps/gui/views/timeline_view.cpp
 * Purpose: Render and interact with the cached G05 scheduling timeline.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Tick-to-pixel conversion is presentation-only; canonical time stays integer.
 ***/

#include "timeline_view.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

constexpr double minimum_pixels_per_tick = 0.0001;
constexpr double maximum_pixels_per_tick = 1000.0;
constexpr float marker_hit_radius = 6.0F;

const char* event_name(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return "release";
    case EventType::JobStart:
        return "start";
    case EventType::JobPreempt:
        return "preempt";
    case EventType::JobResume:
        return "resume";
    case EventType::JobFinish:
        return "finish";
    case EventType::DeadlineMiss:
        return "deadline miss";
    case EventType::MessageSend:
        return "message send";
    case EventType::MessageDelivery:
        return "message delivery";
    }
    return "event";
}

std::string job_label(JobIdentity job) {
    return "T" + std::to_string(job.task_id().value()) + ":J" +
           std::to_string(job.job_id().value());
}

bool point_inside(ImVec2 point, ImVec2 minimum, ImVec2 maximum) {
    return point.x >= minimum.x && point.x <= maximum.x && point.y >= minimum.y &&
           point.y <= maximum.y;
}

bool interval_matches_filter(const GuiTimelineInterval& interval, const GuiSelection& selection) {
    switch (selection.kind()) {
    case GuiSelectionKind::Task:
        return interval.job.task_id() == *selection.task_id();
    case GuiSelectionKind::Resource:
        return interval.resource_id == *selection.resource_id();
    case GuiSelectionKind::Job:
        return interval.job == *selection.job();
    case GuiSelectionKind::None:
    case GuiSelectionKind::Experiment:
    case GuiSelectionKind::Route:
    case GuiSelectionKind::Event:
        return true;
    }
    return true;
}

bool marker_matches_filter(const GuiTimelineMarker& marker, const GuiSelection& selection) {
    switch (selection.kind()) {
    case GuiSelectionKind::Task:
        return marker.task_id == selection.task_id();
    case GuiSelectionKind::Resource:
        return marker.resource_id == selection.resource_id();
    case GuiSelectionKind::Job:
        return marker.task_id == selection.job()->task_id() &&
               marker.job_id == selection.job()->job_id();
    case GuiSelectionKind::Event:
        return marker.sequence == *selection.event_sequence();
    case GuiSelectionKind::None:
    case GuiSelectionKind::Experiment:
    case GuiSelectionKind::Route:
        return true;
    }
    return true;
}

Tick bounded_tick(double value) {
    if (std::isnan(value)) {
        return 0;
    }
    if (value >= static_cast<double>(std::numeric_limits<Tick>::max())) {
        return std::numeric_limits<Tick>::max();
    }
    if (value <= static_cast<double>(std::numeric_limits<Tick>::min())) {
        return std::numeric_limits<Tick>::min();
    }
    return static_cast<Tick>(value);
}

double choose_grid_step(double pixels_per_tick) {
    const auto target_ticks = 90.0 / pixels_per_tick;
    const auto magnitude = std::pow(10.0, std::floor(std::log10(std::max(target_ticks, 1e-9))));
    const auto normalized = target_ticks / magnitude;
    const auto multiplier = normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : 5.0);
    return multiplier * magnitude;
}

float tick_x(Tick tick, float origin_x, const TimelineViewState& state) {
    return origin_x + static_cast<float>((static_cast<double>(tick) - state.view_begin_tick) *
                                         state.pixels_per_tick);
}

void fit_timeline(TimelineViewState& state, const GuiTimelineModel& timeline, float width) {
    auto last_tick = std::max<Tick>(timeline.current_tick, 1);
    if (!timeline.markers.empty()) {
        last_tick = std::max(last_tick, timeline.markers.back().tick);
    }
    state.view_begin_tick = 0.0;
    state.pixels_per_tick = std::clamp(static_cast<double>(std::max(width, 1.0F)) /
                                           static_cast<double>(std::max<Tick>(last_tick, 1)),
                                       minimum_pixels_per_tick, maximum_pixels_per_tick);
    state.fit_requested = false;
}

void draw_marker(ImDrawList* draw_list, const GuiTimelineMarker& marker, ImVec2 center,
                 bool selected) {
    auto color = ImGui::GetColorU32(selected ? ImGuiCol_HeaderHovered : ImGuiCol_PlotLines);
    if (marker.type == EventType::DeadlineMiss) {
        color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    }
    const auto radius = selected ? 5.0F : 4.0F;
    switch (marker.type) {
    case EventType::JobRelease:
    case EventType::MessageSend:
        draw_list->AddTriangleFilled({center.x, center.y - radius},
                                     {center.x - radius, center.y + radius},
                                     {center.x + radius, center.y + radius}, color);
        break;
    case EventType::JobFinish:
    case EventType::MessageDelivery:
        draw_list->AddRectFilled({center.x - radius, center.y - radius},
                                 {center.x + radius, center.y + radius}, color);
        break;
    case EventType::DeadlineMiss:
        draw_list->AddLine({center.x - radius, center.y - radius},
                           {center.x + radius, center.y + radius}, color, 2.0F);
        draw_list->AddLine({center.x - radius, center.y + radius},
                           {center.x + radius, center.y - radius}, color, 2.0F);
        break;
    case EventType::JobStart:
    case EventType::JobPreempt:
    case EventType::JobResume:
        draw_list->AddCircleFilled(center, radius, color);
        break;
    }
}

std::optional<std::size_t> resource_row(const GuiTimelineModel& timeline,
                                        std::optional<ResourceId> resource_id) {
    if (!resource_id.has_value()) {
        return std::nullopt;
    }
    const auto found = std::lower_bound(timeline.rows.begin(), timeline.rows.end(), *resource_id,
                                        [](const GuiTimelineRow& row, ResourceId candidate) {
                                            return row.resource_id < candidate;
                                        });
    if (found == timeline.rows.end() || found->resource_id != *resource_id) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(timeline.rows.begin(), found));
}

} // namespace

bool draw_timeline_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                        TimelineViewState& state) {
    const auto& result =
        state.cache.update(snapshot.event_log, snapshot.experiment, snapshot.current_tick);
    if (!result.valid()) {
        ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.35F, 1.0F}, "Timeline unavailable");
        ImGui::TextWrapped("%s", result.diagnostics.front().message.c_str());
        return false;
    }
    const auto& timeline = *result.timeline;

    if (ImGui::Button("Fit")) {
        state.fit_requested = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Ready", &state.show_ready);
    ImGui::SameLine();
    ImGui::Checkbox("Running", &state.show_running);
    ImGui::SameLine();
    ImGui::Checkbox("Events", &state.show_markers);
    ImGui::SameLine();
    ImGui::Checkbox("Filter selection", &state.filter_to_selection);
    auto zoom_to_time_selection = false;
    if (selection.tick_range().has_value()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Zoom time")) {
            zoom_to_time_selection = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear time")) {
            selection.clear_tick_range();
        }
    }
    ImGui::TextDisabled(
        "Filled: Running | outline: Ready | triangle: release/send | square: finish/delivery | "
        "circle: schedule | X: deadline | wheel: zoom | middle-drag: pan");

    auto canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.x = std::max(canvas_size.x, 80.0F);
    canvas_size.y = std::max(canvas_size.y, 70.0F);
    const auto canvas_origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Timeline canvas input", canvas_size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const auto hovered = ImGui::IsItemHovered();
    const auto mouse = ImGui::GetIO().MousePos;

    const auto label_width = std::min(11.0F * ImGui::GetFontSize(), canvas_size.x * 0.32F);
    const auto plot_origin_x = canvas_origin.x + label_width;
    const auto plot_width = std::max(canvas_size.x - label_width, 1.0F);
    const auto header_height = 2.0F * ImGui::GetTextLineHeightWithSpacing();
    const auto preferred_row_height = std::max(2.2F * ImGui::GetTextLineHeightWithSpacing(), 30.0F);
    const auto row_height =
        timeline.rows.empty()
            ? preferred_row_height
            : std::max(22.0F, std::min(preferred_row_height,
                                       (canvas_size.y - header_height) /
                                           static_cast<float>(timeline.rows.size())));

    if (state.fit_requested) {
        fit_timeline(state, timeline, plot_width);
    }
    if (zoom_to_time_selection && selection.tick_range().has_value()) {
        const auto range = *selection.tick_range();
        const auto span = std::max(
            static_cast<double>(range.end_tick) - static_cast<double>(range.begin_tick), 1.0);
        state.view_begin_tick = static_cast<double>(range.begin_tick);
        state.pixels_per_tick = std::clamp(static_cast<double>(plot_width) / span,
                                           minimum_pixels_per_tick, maximum_pixels_per_tick);
    }
    if (hovered && mouse.x >= plot_origin_x && ImGui::GetIO().MouseWheel != 0.0F) {
        const auto anchor_tick =
            state.view_begin_tick +
            static_cast<double>(mouse.x - plot_origin_x) / state.pixels_per_tick;
        const auto factor = ImGui::GetIO().MouseWheel > 0.0F ? 1.18 : (1.0 / 1.18);
        state.pixels_per_tick = std::clamp(state.pixels_per_tick * factor, minimum_pixels_per_tick,
                                           maximum_pixels_per_tick);
        state.view_begin_tick =
            anchor_tick - static_cast<double>(mouse.x - plot_origin_x) / state.pixels_per_tick;
    }
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
        state.view_begin_tick -=
            static_cast<double>(ImGui::GetIO().MouseDelta.x) / state.pixels_per_tick;
    }

    const auto visible_begin = state.view_begin_tick;
    const auto visible_end =
        state.view_begin_tick + static_cast<double>(plot_width) / state.pixels_per_tick;
    auto* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_max{canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y};
    draw_list->PushClipRect(canvas_origin, canvas_max, true);
    draw_list->AddRectFilled(canvas_origin, canvas_max, ImGui::GetColorU32(ImGuiCol_WindowBg));
    draw_list->AddLine({plot_origin_x, canvas_origin.y}, {plot_origin_x, canvas_max.y},
                       ImGui::GetColorU32(ImGuiCol_Border));

    if (const auto range = selection.tick_range(); range.has_value()) {
        const auto left = tick_x(range->begin_tick, plot_origin_x, state);
        const auto right = tick_x(range->end_tick, plot_origin_x, state);
        draw_list->AddRectFilled({std::min(left, right), canvas_origin.y},
                                 {std::max(left, right) + 1.0F, canvas_max.y},
                                 ImGui::GetColorU32(ImGuiCol_Header, 0.22F));
    }

    const auto grid_step = choose_grid_step(state.pixels_per_tick);
    auto grid_tick = std::ceil(visible_begin / grid_step) * grid_step;
    for (; grid_tick <= visible_end; grid_tick += grid_step) {
        const auto x = plot_origin_x + static_cast<float>((grid_tick - state.view_begin_tick) *
                                                          state.pixels_per_tick);
        draw_list->AddLine({x, canvas_origin.y}, {x, canvas_max.y},
                           ImGui::GetColorU32(ImGuiCol_Border, 0.45F));
        const auto label = std::to_string(bounded_tick(grid_tick));
        draw_list->AddText({x + 3.0F, canvas_origin.y + 2.0F},
                           ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
    }

    draw_list->AddText({canvas_origin.x + 5.0F, canvas_origin.y + 2.0F},
                       ImGui::GetColorU32(ImGuiCol_Text), "Global events");
    auto selection_hit = false;
    for (std::size_t index = 0; index < timeline.rows.size(); ++index) {
        const auto top = canvas_origin.y + header_height + static_cast<float>(index) * row_height;
        if (top > canvas_max.y) {
            break;
        }
        const auto& row = timeline.rows[index];
        const auto label = "R" + std::to_string(row.resource_id.value()) + " " + row.label;
        draw_list->AddText({canvas_origin.x + 5.0F, top + 6.0F},
                           ImGui::GetColorU32(selection.resource_id() == row.resource_id
                                                  ? ImGuiCol_HeaderHovered
                                                  : ImGuiCol_Text),
                           label.c_str());
        draw_list->AddLine({canvas_origin.x, top}, {canvas_max.x, top},
                           ImGui::GetColorU32(ImGuiCol_Border, 0.55F));

        for (const auto& interval : row.intervals) {
            if ((interval.kind == GuiTimelineIntervalKind::Ready && !state.show_ready) ||
                (interval.kind == GuiTimelineIntervalKind::Running && !state.show_running) ||
                (state.filter_to_selection && !interval_matches_filter(interval, selection))) {
                continue;
            }
            const auto end_tick = interval.end_tick.value_or(timeline.current_tick);
            if (static_cast<double>(end_tick) < visible_begin ||
                static_cast<double>(interval.begin_tick) > visible_end) {
                continue;
            }
            const auto left = tick_x(interval.begin_tick, plot_origin_x, state);
            const auto right = std::max(tick_x(end_tick, plot_origin_x, state), left + 3.0F);
            const ImVec2 minimum{left, top + 9.0F};
            const ImVec2 maximum{right, top + row_height - 7.0F};
            const auto selected = selection.job() == interval.job;
            const auto color =
                ImGui::GetColorU32(selected ? ImGuiCol_HeaderHovered : ImGuiCol_PlotHistogram);
            if (interval.kind == GuiTimelineIntervalKind::Running) {
                draw_list->AddRectFilled(minimum, maximum, color, 2.0F);
            } else {
                draw_list->AddRect(minimum, maximum, color, 2.0F, 0, selected ? 3.0F : 1.8F);
            }
            if (right - left > 32.0F) {
                const auto label_text = job_label(interval.job);
                draw_list->AddText({left + 4.0F, minimum.y + 2.0F},
                                   ImGui::GetColorU32(ImGuiCol_Text), label_text.c_str());
            }
            if (hovered && point_inside(mouse, minimum, maximum)) {
                ImGui::SetTooltip(
                    "%s %s [%lld, %lld%s", job_label(interval.job).c_str(),
                    interval.kind == GuiTimelineIntervalKind::Running ? "Running" : "Ready",
                    static_cast<long long>(interval.begin_tick), static_cast<long long>(end_tick),
                    interval.end_tick.has_value() ? ")" : ", live)");
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selection.select_job(interval.job);
                    selection.select_tick_range(
                        {.begin_tick = interval.begin_tick, .end_tick = end_tick});
                    selection_hit = true;
                }
            }
        }
    }

    if (state.show_markers) {
        const auto first =
            std::lower_bound(timeline.markers.begin(), timeline.markers.end(), visible_begin,
                             [](const GuiTimelineMarker& marker, double tick) {
                                 return static_cast<double>(marker.tick) < tick;
                             });
        for (auto marker = first;
             marker != timeline.markers.end() && static_cast<double>(marker->tick) <= visible_end;
             ++marker) {
            if (state.filter_to_selection && !marker_matches_filter(*marker, selection)) {
                continue;
            }
            const auto row_index = resource_row(timeline, marker->resource_id);
            const auto y = row_index.has_value()
                               ? canvas_origin.y + header_height +
                                     static_cast<float>(*row_index) * row_height + 7.0F
                               : canvas_origin.y + header_height - 8.0F;
            if (y > canvas_max.y) {
                continue;
            }
            const ImVec2 center{tick_x(marker->tick, plot_origin_x, state), y};
            const auto selected = selection.event_sequence() == marker->sequence;
            draw_marker(draw_list, *marker, center, selected);
            if (hovered &&
                std::hypot(mouse.x - center.x, mouse.y - center.y) <= marker_hit_radius) {
                ImGui::SetTooltip("event %llu: %s at tick %lld",
                                  static_cast<unsigned long long>(marker->sequence.value()),
                                  event_name(marker->type), static_cast<long long>(marker->tick));
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selection.select_event(marker->sequence);
                    selection.select_tick(marker->tick);
                    selection_hit = true;
                }
            }
        }
    }

    const auto cursor_x = tick_x(timeline.current_tick, plot_origin_x, state);
    draw_list->AddLine({cursor_x, canvas_origin.y}, {cursor_x, canvas_max.y},
                       ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0F);
    draw_list->PopClipRect();

    if (hovered && mouse.x >= plot_origin_x && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !selection_hit) {
        const auto tick =
            bounded_tick(state.view_begin_tick +
                         static_cast<double>(mouse.x - plot_origin_x) / state.pixels_per_tick);
        selection.select_tick(std::clamp<Tick>(tick, 0, timeline.current_tick));
    }
    return hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}

} // namespace cpssim::gui
