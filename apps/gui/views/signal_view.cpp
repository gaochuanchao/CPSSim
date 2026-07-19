/***
 * File: apps/gui/views/signal_view.cpp
 * Purpose: Render G06 scalar plots with shared integer-tick selection.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Dear ImGui draw coordinates are floating-point; stored sample ticks are not.
 ***/

#include "signal_view.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace cpssim::gui {
namespace {

constexpr double minimum_view_span = 1.0;

bool selected(const SignalViewState& state, const GuiSignalId& id) {
    return std::find(state.selected_signals.begin(), state.selected_signals.end(), id) !=
           state.selected_signals.end();
}

void set_selected(SignalViewState& state, const GuiSignalId& id, bool enabled) {
    const auto found = std::find(state.selected_signals.begin(), state.selected_signals.end(), id);
    if (enabled && found == state.selected_signals.end()) {
        state.selected_signals.push_back(id);
        std::sort(state.selected_signals.begin(), state.selected_signals.end());
    } else if (!enabled && found != state.selected_signals.end()) {
        state.selected_signals.erase(found);
    }
}

const char* scalar_type_name(GuiSignalScalarType type) {
    switch (type) {
    case GuiSignalScalarType::Real:
        return "Real";
    case GuiSignalScalarType::Integer:
        return "Integer";
    case GuiSignalScalarType::Boolean:
        return "Boolean";
    }
    return "Unknown";
}

std::string format_value(const GuiScalarValue& value) {
    if (const auto* real = std::get_if<double>(&value)) {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%.7g", *real);
        return buffer;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*integer);
    }
    return std::get<bool>(value) ? "true" : "false";
}

const GuiScalarSample* sample_at_tick(const GuiSignalSeries& series, Tick tick) {
    const auto found = std::lower_bound(
        series.samples.begin(), series.samples.end(), tick,
        [](const GuiScalarSample& sample, Tick candidate) { return sample.tick < candidate; });
    return found != series.samples.end() && found->tick == tick ? &*found : nullptr;
}

Tick bounded_tick(double value, Tick maximum) {
    if (std::isnan(value) || value <= 0.0) {
        return 0;
    }
    if (value >= static_cast<double>(maximum)) {
        return maximum;
    }
    return static_cast<Tick>(value);
}

double grid_step(double span, float width) {
    const auto target = span * 90.0 / static_cast<double>(std::max(width, 1.0F));
    const auto magnitude = std::pow(10.0, std::floor(std::log10(std::max(target, 1e-9))));
    const auto normalized = target / magnitude;
    return (normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : 5.0)) * magnitude;
}

ImU32 series_color(std::size_t index) {
    constexpr std::array<ImVec4, 8> colors{{{0.25F, 0.75F, 1.0F, 1.0F},
                                            {1.0F, 0.58F, 0.25F, 1.0F},
                                            {0.45F, 0.86F, 0.45F, 1.0F},
                                            {0.88F, 0.45F, 0.85F, 1.0F},
                                            {1.0F, 0.84F, 0.3F, 1.0F},
                                            {0.35F, 0.85F, 0.78F, 1.0F},
                                            {0.95F, 0.42F, 0.48F, 1.0F},
                                            {0.72F, 0.68F, 1.0F, 1.0F}}};
    return ImGui::ColorConvertFloat4ToU32(colors[index % colors.size()]);
}

void fit_to_data(SignalViewState& state, const GuiSignalModel& model) {
    Tick last_tick = 1;
    for (const auto& series : model.series) {
        if (!series.samples.empty()) {
            last_tick = std::max(last_tick, series.samples.back().tick);
        }
    }
    state.view_begin_tick = 0.0;
    state.view_end_tick = static_cast<double>(last_tick);
    state.fit_requested = false;
}

} // namespace

void draw_signal_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                      SignalViewState& state) {
    const auto& result =
        state.cache.update(snapshot.functional_observations, snapshot.functional_signal_registry);
    if (!result.valid()) {
        ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.35F, 1.0F}, "Signals unavailable");
        ImGui::TextWrapped("%s", result.diagnostics.front().message.c_str());
        return;
    }
    const auto& model = *result.model;
    if (!snapshot.functional_model_attached) {
        ImGui::TextDisabled("No functional model is attached to this run.");
        ImGui::TextWrapped(
            "Scheduling and canonical-event views remain available; attach a functional-model "
            "factory to the GUI session to publish scalar observations.");
        return;
    }
    if (snapshot.functional_observations.empty()) {
        ImGui::TextDisabled(snapshot.run_state == GuiRunState::NotConfigured
                                ? "Apply the run plan to create the functional run."
                                : "Functional model attached; Step or Run to initialize signals.");
        return;
    }

    state.selected_signals.erase(std::remove_if(state.selected_signals.begin(),
                                                state.selected_signals.end(),
                                                [&model](const GuiSignalId& id) {
                                                    return find_signal_series(model, id) == nullptr;
                                                }),
                                 state.selected_signals.end());
    if (!state.selection_initialized) {
        state.selected_signals = {model.series.front().descriptor.id};
        state.selection_initialized = true;
    }

    if (ImGui::Button("Fit data")) {
        state.fit_requested = true;
        state.auto_follow = false;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-follow", &state.auto_follow);
    if (selection.tick_range().has_value()) {
        ImGui::SameLine();
        if (ImGui::Button("Zoom time")) {
            const auto range = *selection.tick_range();
            state.view_begin_tick = static_cast<double>(range.begin_tick);
            state.view_end_tick = std::max(static_cast<double>(range.end_tick),
                                           static_cast<double>(range.begin_tick) + 1.0);
            state.auto_follow = false;
            state.fit_requested = false;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("wheel: zoom | middle-drag: pan | left-drag: select time");

    const auto full_size = ImGui::GetContentRegionAvail();
    const auto selector_width = std::min(15.0F * ImGui::GetFontSize(), full_size.x * 0.34F);
    if (ImGui::BeginTable("Signal plot layout", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                          ImVec2{0.0F, 0.0F})) {
        ImGui::TableSetupColumn("Signals", ImGuiTableColumnFlags_WidthFixed, selector_width);
        ImGui::TableSetupColumn("Plot", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("Signal selector", ImVec2{0.0F, 0.0F});
        ImGui::TextUnformatted("Available signals");
        ImGui::Separator();
        for (const auto& series : model.series) {
            auto enabled = selected(state, series.descriptor.id);
            ImGui::PushID(series.descriptor.path.c_str());
            if (ImGui::Checkbox(series.descriptor.display_name.c_str(), &enabled)) {
                set_selected(state, series.descriptor.id, enabled);
            }
            ImGui::TextDisabled("%s%s%s", scalar_type_name(series.descriptor.id.scalar_type),
                                series.descriptor.unit.empty() ? "" : " | ",
                                series.descriptor.unit.c_str());
            if (const auto range = selection.tick_range();
                range.has_value() && range->begin_tick == range->end_tick) {
                if (const auto* sample = sample_at_tick(series, range->begin_tick);
                    sample != nullptr) {
                    ImGui::Text("t=%lld: %s", static_cast<long long>(sample->tick),
                                format_value(sample->value).c_str());
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        auto canvas_size = ImGui::GetContentRegionAvail();
        canvas_size.x = std::max(canvas_size.x, 80.0F);
        canvas_size.y = std::max(canvas_size.y, 80.0F);
        const auto canvas_origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Signal canvas input", canvas_size,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const auto hovered = ImGui::IsItemHovered();
        const auto mouse = ImGui::GetIO().MousePos;

        if (state.fit_requested) {
            fit_to_data(state, model);
        }
        const auto newest_tick = snapshot.functional_observations.empty()
                                     ? Tick{0}
                                     : snapshot.functional_observations.back().tick;
        if (state.auto_follow) {
            const auto span =
                std::max(state.view_end_tick - state.view_begin_tick, minimum_view_span);
            state.view_end_tick = static_cast<double>(std::max<Tick>(newest_tick, 1));
            state.view_begin_tick = std::max(0.0, state.view_end_tick - span);
        }
        if (state.view_end_tick <= state.view_begin_tick) {
            state.view_end_tick = state.view_begin_tick + minimum_view_span;
        }

        const auto plot_left = canvas_origin.x + 58.0F;
        const auto plot_right = canvas_origin.x + canvas_size.x - 8.0F;
        const auto plot_top = canvas_origin.y + 20.0F;
        const auto plot_bottom = canvas_origin.y + canvas_size.y - 28.0F;
        const auto plot_width = std::max(plot_right - plot_left, 1.0F);
        const auto plot_height = std::max(plot_bottom - plot_top, 1.0F);
        auto span = std::max(state.view_end_tick - state.view_begin_tick, minimum_view_span);

        if (hovered && mouse.x >= plot_left && ImGui::GetIO().MouseWheel != 0.0F) {
            const auto anchor = state.view_begin_tick + static_cast<double>(mouse.x - plot_left) /
                                                            static_cast<double>(plot_width) * span;
            const auto factor = ImGui::GetIO().MouseWheel > 0.0F ? 0.82 : (1.0 / 0.82);
            const auto new_span = std::max(span * factor, minimum_view_span);
            const auto fraction =
                static_cast<double>(mouse.x - plot_left) / static_cast<double>(plot_width);
            state.view_begin_tick = anchor - fraction * new_span;
            state.view_end_tick = state.view_begin_tick + new_span;
            state.auto_follow = false;
            span = new_span;
        }
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
            const auto delta = static_cast<double>(ImGui::GetIO().MouseDelta.x) /
                               static_cast<double>(plot_width) * span;
            state.view_begin_tick -= delta;
            state.view_end_tick -= delta;
            state.auto_follow = false;
        }
        if (state.view_begin_tick < 0.0) {
            state.view_end_tick -= state.view_begin_tick;
            state.view_begin_tick = 0.0;
        }

        const auto visible_begin = bounded_tick(state.view_begin_tick, newest_tick);
        const auto visible_end = bounded_tick(state.view_end_tick, newest_tick);
        const auto maximum_points =
            std::max<std::size_t>(static_cast<std::size_t>(plot_width) * 2, 4);
        std::vector<std::pair<const GuiSignalSeries*, std::vector<GuiScalarSample>>> visible;
        auto value_minimum = std::numeric_limits<double>::infinity();
        auto value_maximum = -std::numeric_limits<double>::infinity();
        for (const auto& id : state.selected_signals) {
            const auto* series = find_signal_series(model, id);
            if (series == nullptr) {
                continue;
            }
            auto samples = downsample_signal(*series, visible_begin, visible_end, maximum_points);
            for (const auto& sample : samples) {
                const auto value = gui_scalar_as_double(sample.value);
                value_minimum = std::min(value_minimum, value);
                value_maximum = std::max(value_maximum, value);
            }
            visible.emplace_back(series, std::move(samples));
        }
        if (!std::isfinite(value_minimum) || !std::isfinite(value_maximum)) {
            value_minimum = 0.0;
            value_maximum = 1.0;
        } else if (value_minimum == value_maximum) {
            value_minimum -= 0.5;
            value_maximum += 0.5;
        }

        auto* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 canvas_max{canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y};
        draw_list->PushClipRect(canvas_origin, canvas_max, true);
        draw_list->AddRectFilled(canvas_origin, canvas_max, ImGui::GetColorU32(ImGuiCol_WindowBg));
        draw_list->AddRect({plot_left, plot_top}, {plot_right, plot_bottom},
                           ImGui::GetColorU32(ImGuiCol_Border));

        const auto tick_to_x = [&](Tick tick) {
            return plot_left +
                   static_cast<float>((static_cast<double>(tick) - state.view_begin_tick) / span) *
                       plot_width;
        };
        const auto value_to_y = [&](double value) {
            return plot_bottom -
                   static_cast<float>((value - value_minimum) / (value_maximum - value_minimum)) *
                       plot_height;
        };

        const auto step = grid_step(span, plot_width);
        auto grid_tick = std::ceil(state.view_begin_tick / step) * step;
        for (; grid_tick <= state.view_end_tick; grid_tick += step) {
            const auto x =
                plot_left +
                static_cast<float>((grid_tick - state.view_begin_tick) / span) * plot_width;
            draw_list->AddLine({x, plot_top}, {x, plot_bottom},
                               ImGui::GetColorU32(ImGuiCol_Border, 0.45F));
            const auto label = std::to_string(bounded_tick(grid_tick, newest_tick));
            draw_list->AddText({x + 2.0F, plot_bottom + 4.0F},
                               ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
        }
        char maximum_label[48]{};
        char minimum_label[48]{};
        std::snprintf(maximum_label, sizeof(maximum_label), "%.5g", value_maximum);
        std::snprintf(minimum_label, sizeof(minimum_label), "%.5g", value_minimum);
        draw_list->AddText({canvas_origin.x + 3.0F, plot_top},
                           ImGui::GetColorU32(ImGuiCol_TextDisabled), maximum_label);
        draw_list->AddText({canvas_origin.x + 3.0F, plot_bottom - ImGui::GetFontSize()},
                           ImGui::GetColorU32(ImGuiCol_TextDisabled), minimum_label);

        if (const auto range = selection.tick_range(); range.has_value()) {
            const auto left = tick_to_x(range->begin_tick);
            const auto right = tick_to_x(range->end_tick);
            if (range->begin_tick == range->end_tick) {
                draw_list->AddLine({left, plot_top}, {left, plot_bottom},
                                   ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0F);
            } else {
                draw_list->AddRectFilled({std::min(left, right), plot_top},
                                         {std::max(left, right), plot_bottom},
                                         ImGui::GetColorU32(ImGuiCol_Header, 0.2F));
            }
        }

        for (std::size_t series_index = 0; series_index < visible.size(); ++series_index) {
            const auto& [series, samples] = visible[series_index];
            const auto color = series_color(series_index);
            for (std::size_t index = 1; index < samples.size(); ++index) {
                draw_list->AddLine({tick_to_x(samples[index - 1].tick),
                                    value_to_y(gui_scalar_as_double(samples[index - 1].value))},
                                   {tick_to_x(samples[index].tick),
                                    value_to_y(gui_scalar_as_double(samples[index].value))},
                                   color, 1.8F);
            }
            if (!samples.empty()) {
                draw_list->AddCircleFilled({tick_to_x(samples.back().tick),
                                            value_to_y(gui_scalar_as_double(samples.back().value))},
                                           2.5F, color);
            }
            draw_list->AddText(
                {plot_left + 8.0F,
                 plot_top + 4.0F + static_cast<float>(series_index) * ImGui::GetTextLineHeight()},
                color, series->descriptor.display_name.c_str());
        }
        draw_list->PopClipRect();

        const auto inside_plot = hovered && mouse.x >= plot_left && mouse.x <= plot_right &&
                                 mouse.y >= plot_top && mouse.y <= plot_bottom;
        const auto mouse_tick =
            bounded_tick(state.view_begin_tick + static_cast<double>(mouse.x - plot_left) /
                                                     static_cast<double>(plot_width) * span,
                         newest_tick);
        if (inside_plot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.drag_begin_tick = mouse_tick;
        }
        if (state.drag_begin_tick.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (std::abs(mouse.x - ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left].x) >
                3.0F) {
                selection.select_tick_range(
                    {.begin_tick = *state.drag_begin_tick, .end_tick = mouse_tick});
            } else {
                selection.select_tick(mouse_tick);
            }
            state.drag_begin_tick.reset();
        }
        if (inside_plot) {
            ImGui::BeginTooltip();
            ImGui::Text("Tick %lld", static_cast<long long>(mouse_tick));
            for (const auto& [series, samples] : visible) {
                static_cast<void>(samples);
                if (const auto* sample = sample_at_tick(*series, mouse_tick); sample != nullptr) {
                    ImGui::Text("%s: %s%s%s", series->descriptor.display_name.c_str(),
                                format_value(sample->value).c_str(),
                                series->descriptor.unit.empty() ? "" : " ",
                                series->descriptor.unit.c_str());
                }
            }
            ImGui::EndTooltip();
        }
        ImGui::EndTable();
    }
}

} // namespace cpssim::gui
