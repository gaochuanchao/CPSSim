/*** Render a filtered, virtualized table over the detached canonical trace. ***/

#include "event_view.hpp"

#include "cpssim/gui/event_table_model.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>

namespace cpssim::gui {
namespace {

template <typename Identifier>
void draw_optional_identifier(const std::optional<Identifier>& identifier) {
    if (identifier.has_value()) {
        ImGui::Text("%llu", static_cast<unsigned long long>(identifier->value()));
    } else {
        ImGui::TextDisabled("—");
    }
}

void draw_column_menu(GuiEventColumnVisibility& columns) {
    if (!ImGui::BeginPopup("Event columns")) {
        return;
    }
    ImGui::MenuItem("Sequence", nullptr, &columns.sequence);
    ImGui::MenuItem("Tick", nullptr, &columns.tick);
    ImGui::MenuItem("Time", nullptr, &columns.time);
    ImGui::MenuItem("Type", nullptr, &columns.type);
    ImGui::MenuItem("Phase", nullptr, &columns.phase);
    ImGui::MenuItem("Task", nullptr, &columns.task);
    ImGui::MenuItem("Job", nullptr, &columns.job);
    ImGui::MenuItem("Resource", nullptr, &columns.resource);
    ImGui::MenuItem("Message", nullptr, &columns.message);
    ImGui::MenuItem("Vehicle", nullptr, &columns.vehicle);
    ImGui::MenuItem("Cause", nullptr, &columns.cause);
    ImGui::EndPopup();
}

void draw_filters(GuiEventFilters& filters, GuiEventColumnVisibility& columns,
                  EventViewState& state) {
    if (!state.filter_initialized) {
        const auto count = std::min(filters.text.size(), state.text_filter.size() - 1);
        std::copy_n(filters.text.begin(), count, state.text_filter.begin());
        state.text_filter[count] = '\0';
        state.filter_initialized = true;
    }
    constexpr const char* type_names[] = {"All types",     "Job release",  "Job start",
                                          "Job preempt",   "Job resume",   "Job finish",
                                          "Deadline miss", "Message send", "Message delivery"};
    auto type_index = filters.type.has_value() ? static_cast<int>(*filters.type) + 1 : 0;
    ImGui::SetNextItemWidth(10.0F * ImGui::GetFontSize());
    if (ImGui::Combo("##event_type", &type_index, type_names,
                     static_cast<int>(std::size(type_names)))) {
        filters.type = type_index == 0
                           ? std::nullopt
                           : std::optional<EventType>{static_cast<EventType>(type_index - 1)};
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(8.0F * ImGui::GetFontSize(), ImGui::GetContentRegionAvail().x -
                                                                      7.0F * ImGui::GetFontSize()));
    if (ImGui::InputTextWithHint("##event_text", "Filter text...", state.text_filter.data(),
                                 state.text_filter.size())) {
        filters.text = state.text_filter.data();
    }
    ImGui::SameLine();
    if (ImGui::Button("Columns")) {
        ImGui::OpenPopup("Event columns");
    }
    draw_column_menu(columns);

    const auto id_filter = [](const char* label, auto& value) {
        auto enabled = value.has_value();
        ImGui::Checkbox(label, &enabled);
        ImGui::SameLine();
        std::uint64_t number = value.has_value() ? value->value() : 0;
        ImGui::BeginDisabled(!enabled);
        ImGui::SetNextItemWidth(5.5F * ImGui::GetFontSize());
        if (ImGui::InputScalar((std::string{"##"} + label).c_str(), ImGuiDataType_U64, &number)) {
            value = typename std::remove_reference_t<decltype(value)>::value_type{number};
        }
        ImGui::EndDisabled();
        if (!enabled) {
            value.reset();
        } else if (!value.has_value()) {
            value = typename std::remove_reference_t<decltype(value)>::value_type{number};
        }
    };
    id_filter("Task", filters.task);
    ImGui::SameLine();
    id_filter("Resource", filters.resource);
    ImGui::SameLine();
    id_filter("Vehicle", filters.vehicle);
}

} // namespace

void draw_event_view(const SimulationSnapshot& snapshot, GuiSelection& selection,
                     GuiEventFilters& filters, GuiEventColumnVisibility& columns,
                     EventViewState& state) {
    draw_filters(filters, columns, state);
    const auto rows = build_event_table_rows(snapshot);
    const auto projected = filter_event_table_rows(rows, filters);
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                       ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollX |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("Canonical event table", 11, flags, ImVec2{0.0F, 0.0F})) {
        return;
    }
    ImGui::TableSetupColumn("Sequence");
    ImGui::TableSetupColumn("Tick");
    ImGui::TableSetupColumn("Time");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Phase");
    ImGui::TableSetupColumn("Task");
    ImGui::TableSetupColumn("Job");
    ImGui::TableSetupColumn("Resource");
    ImGui::TableSetupColumn("Message");
    ImGui::TableSetupColumn("Vehicle");
    ImGui::TableSetupColumn("Cause");
    const bool enabled_columns[]{columns.sequence, columns.tick,    columns.time, columns.type,
                                 columns.phase,    columns.task,    columns.job,  columns.resource,
                                 columns.message,  columns.vehicle, columns.cause};
    for (int column = 0; column < static_cast<int>(std::size(enabled_columns)); ++column) {
        ImGui::TableSetColumnEnabled(column, enabled_columns[column]);
    }
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(projected.size()));
    while (clipper.Step()) {
        for (auto visible = clipper.DisplayStart; visible < clipper.DisplayEnd; ++visible) {
            const auto& row = rows[projected[static_cast<std::size_t>(visible)]];
            ImGui::PushID(static_cast<int>(row.sequence.value()));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const auto sequence = std::to_string(row.sequence.value());
            if (ImGui::Selectable(sequence.c_str(), selection.event_sequence() == row.sequence,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                selection.select_event(row.sequence);
                selection.select_tick(row.tick);
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%lld", static_cast<long long>(row.tick));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.6f ms", row.time_milliseconds);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(row.type_name.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(row.phase_name.c_str());
            ImGui::TableSetColumnIndex(5);
            draw_optional_identifier(row.entities.task_id);
            ImGui::TableSetColumnIndex(6);
            draw_optional_identifier(row.entities.job_id);
            ImGui::TableSetColumnIndex(7);
            draw_optional_identifier(row.entities.resource_id);
            ImGui::TableSetColumnIndex(8);
            draw_optional_identifier(row.entities.message_id);
            ImGui::TableSetColumnIndex(9);
            draw_optional_identifier(row.entities.vehicle_id);
            ImGui::TableSetColumnIndex(10);
            if (row.cause.has_value()) {
                const auto cause = std::to_string(row.cause->value());
                if (ImGui::SmallButton(cause.c_str())) {
                    selection.select_event(*row.cause);
                    if (const auto predecessor = find_event_row_by_sequence(rows, *row.cause);
                        predecessor.has_value()) {
                        selection.select_tick(rows[*predecessor].tick);
                    }
                }
            } else {
                ImGui::TextDisabled("—");
            }
            if (ImGui::BeginPopupContextItem("Raw event JSON")) {
                ImGui::TextWrapped("%s", row.raw_json.c_str());
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndTable();
}

} // namespace cpssim::gui
