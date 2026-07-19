/***
 * File: apps/gui/views/toolbar_view.cpp
 * Purpose: Render queued simulation controls and current detached run status.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "toolbar_view.hpp"

#include "imgui.h"

#include <stdexcept>

namespace cpssim::gui {
namespace {

/*** Converts one controller state into a short display label. ***/
const char* run_state_name(GuiRunState state) {
    switch (state) {
    case GuiRunState::NotConfigured:
        return "Not configured";
    case GuiRunState::Paused:
        return "Paused";
    case GuiRunState::Running:
        return "Running";
    case GuiRunState::Finished:
        return "Finished";
    }
    throw std::logic_error{"unknown GUI run state"};
}

} // namespace

/*** Enqueues only meaningful existing commands and presents progress inline. ***/
void draw_toolbar(GuiSimulationSession& session, const SimulationSnapshot& snapshot) {
    const auto configured = session.has_active_run();
    const auto finished = snapshot.run_state == GuiRunState::Finished;
    const auto running = snapshot.run_state == GuiRunState::Running;
    const auto reset_available = snapshot.current_tick != 0 || !snapshot.event_log.empty();

    ImGui::BeginDisabled(!configured || running || finished);
    if (ImGui::Button("Run")) {
        session.enqueue(GuiCommand::Run);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!configured || !running || finished);
    if (ImGui::Button("Pause")) {
        session.enqueue(GuiCommand::Pause);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!reset_available);
    if (ImGui::Button("Reset")) {
        session.enqueue(GuiCommand::Reset);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!configured || running || finished);
    if (ImGui::Button("Next event")) {
        session.enqueue(GuiCommand::StepNextEvent);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("State: %s", run_state_name(snapshot.run_state));
    ImGui::SameLine();
    ImGui::Text("Tick: %lld / %lld", static_cast<long long>(snapshot.current_tick),
                static_cast<long long>(snapshot.stop_tick));
    ImGui::SameLine();
    ImGui::Text("Events: %zu", snapshot.event_log.size());
}

} // namespace cpssim::gui
