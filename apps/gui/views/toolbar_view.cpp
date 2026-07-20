/***
 * File: apps/gui/views/toolbar_view.cpp
 * Purpose: Render queued simulation controls and current detached run status.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "toolbar_view.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdint>
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
void draw_toolbar(GuiSimulationSession& session, const SimulationProgress& progress,
                  GuiWorkspaceState& workspace) {
    const auto configured = session.has_active_run();
    const auto finished = progress.run_state == GuiRunState::Finished;
    const auto running = progress.run_state == GuiRunState::Running;
    const auto reset_available = progress.current_tick != 0 || progress.event_count != 0;

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
    ImGui::Text("State: %s", run_state_name(progress.run_state));
    ImGui::SameLine();
    ImGui::Text("Tick: %lld / %lld", static_cast<long long>(progress.current_tick),
                static_cast<long long>(progress.stop_tick));
    ImGui::SameLine();
    ImGui::Text("Events: %llu", static_cast<unsigned long long>(progress.event_count));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(6.5F * ImGui::GetFontSize());
    const auto mode_preview = workspace.run_mode == GuiRunMode::Live ? "Live" : "Fast";
    if (ImGui::BeginCombo("Run mode", mode_preview)) {
        if (ImGui::Selectable("Live", workspace.run_mode == GuiRunMode::Live))
            workspace.run_mode = GuiRunMode::Live;
        if (ImGui::Selectable("Fast", workspace.run_mode == GuiRunMode::Fast))
            workspace.run_mode = GuiRunMode::Fast;
        ImGui::EndCombo();
    }
    if (workspace.run_mode == GuiRunMode::Fast) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(6.5F * ImGui::GetFontSize());
        const auto unit_preview =
            workspace.fast_batch_unit == GuiFastBatchUnit::Events ? "Events" : "Ticks";
        if (ImGui::BeginCombo("Batch unit", unit_preview)) {
            if (ImGui::Selectable("Events", workspace.fast_batch_unit == GuiFastBatchUnit::Events))
                workspace.fast_batch_unit = GuiFastBatchUnit::Events;
            if (ImGui::Selectable("Ticks", workspace.fast_batch_unit == GuiFastBatchUnit::Ticks))
                workspace.fast_batch_unit = GuiFastBatchUnit::Ticks;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        auto& batch = workspace.fast_batch_unit == GuiFastBatchUnit::Events
                          ? workspace.fast_event_batch_size
                          : workspace.fast_tick_batch_size;
        ImGui::SetNextItemWidth(7.0F * ImGui::GetFontSize());
        if (ImGui::InputScalar("Batch size", ImGuiDataType_U64, &batch)) {
            batch = std::clamp<std::uint64_t>(batch, 1, 1'000'000);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset batch settings")) {
            workspace.fast_batch_unit = GuiFastBatchUnit::Events;
            workspace.fast_event_batch_size = 1000;
            workspace.fast_tick_batch_size = 1000;
        }
    }
}

} // namespace cpssim::gui
