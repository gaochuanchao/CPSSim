/***
 * File: apps/gui/views/event_view.cpp
 * Purpose: Render canonical JSON Lines from a detached append-only event copy.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "event_view.hpp"

#include "cpssim/trace/event_json.hpp"

#include "imgui.h"

#include <string>

namespace cpssim::gui {

/*** Draws every event, emphasizing related identities without filtering. ***/
void draw_event_view(const SimulationSnapshot& snapshot, GuiSelection& selection) {
    ImGui::BeginChild("event_log", ImVec2{0.0F, 0.0F}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& event : snapshot.event_log) {
        auto line = serialize_event_json_line(event);
        line.pop_back();
        const auto stable_id = "event:" + std::to_string(event.sequence().value());
        ImGui::PushID(stable_id.c_str());
        if (ImGui::Selectable(line.c_str(), event_matches_selection(event, selection))) {
            selection.select_event(event.sequence());
            selection.select_tick(event.tick());
        }
        if (selection.event_sequence() == event.sequence() && !ImGui::IsItemVisible()) {
            ImGui::SetScrollHereY(0.5F);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

} // namespace cpssim::gui
