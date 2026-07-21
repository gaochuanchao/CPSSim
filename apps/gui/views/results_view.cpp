/*** Render compact finish-only results from the shared completed cache. ***/

#include "results_view.hpp"

#include "cpssim/application/bosch_result_analysis.hpp"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace cpssim::gui {
namespace {

const char* state_name(GuiRunState state) {
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
    return "Unknown";
}

void row(const char* label, std::uint64_t value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(value));
}

void summary(const RunMetrics& metrics) {
    ImGui::BeginChild("Compact run summary",
                      ImVec2{std::min(320.0F, ImGui::GetContentRegionAvail().x), 0.0F},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::SeparatorText("Run Summary");
    if (ImGui::BeginTable("Summary values", 2, ImGuiTableFlags_SizingFixedFit)) {
        row("Canonical events", metrics.event_count);
        row("Completed jobs", metrics.completed_jobs);
        row("Deadline misses", metrics.deadline_misses);
        row("Preemptions", metrics.preemptions);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Horizon");
        ImGui::TableNextColumn();
        ImGui::Text("%lld ticks", static_cast<long long>(metrics.horizon_tick));
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void timing(const RunMetrics& metrics, const RunPerformanceSummary& performance) {
    ImGui::SeparatorText("Timing and Execution");
    const auto paired =
        metrics.messages.delivery_delay.has_value() ? metrics.messages.delivery_delay->count : 0;
    ImGui::Text("Messages: %llu sent | %llu delivered | %llu paired | %llu undelivered",
                static_cast<unsigned long long>(metrics.messages.sent),
                static_cast<unsigned long long>(metrics.messages.delivered),
                static_cast<unsigned long long>(paired),
                static_cast<unsigned long long>(metrics.messages.sent -
                                                std::min(metrics.messages.sent, paired)));
    if (metrics.messages.delivery_delay.has_value()) {
        const auto& delay = *metrics.messages.delivery_delay;
        ImGui::Text("Delivery delay: min %lld | mean %.3f | max %lld ticks",
                    static_cast<long long>(delay.minimum), delay.mean(),
                    static_cast<long long>(delay.maximum));
    } else
        ImGui::TextDisabled("Delivery delay: Unavailable");
    ImGui::Text("Wall time: %.3f s | %.1f events/s | %.1f ticks/s",
                std::chrono::duration<double>(performance.wall_clock_duration).count(),
                performance.events_per_second, performance.ticks_per_second);
    ImGui::Text("Mode: %s", performance.mode == GuiRunMode::Fast ? "Fast" : "Live");
    if (performance.mode == GuiRunMode::Fast) {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s, %llu",
                            performance.batch_unit == GuiFastBatchUnit::Events ? "Events" : "Ticks",
                            static_cast<unsigned long long>(performance.batch_size));
    }
}

void responses(const RunMetrics& metrics) {
    ImGui::SeparatorText("Task Response Times");
    if (!ImGui::BeginTable("Completed task responses", 7,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit))
        return;
    for (const auto* heading :
         {"Task", "Completed jobs", "Minimum", "Mean", "Maximum", "Deadline", "Deadline misses"})
        ImGui::TableSetupColumn(heading);
    ImGui::TableHeadersRow();
    for (const auto& task : metrics.task_responses) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s (T%llu)", task.task_name.c_str(),
                    static_cast<unsigned long long>(task.task_id.value()));
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(task.completed_jobs));
        if (task.response_time.has_value()) {
            ImGui::TableNextColumn();
            ImGui::Text("%lld", static_cast<long long>(task.response_time->minimum));
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", task.response_time->mean());
            ImGui::TableNextColumn();
            ImGui::Text("%lld", static_cast<long long>(task.response_time->maximum));
        } else
            for (int index = 0; index < 3; ++index) {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("Unavailable");
            }
        ImGui::TableNextColumn();
        ImGui::Text("%lld", static_cast<long long>(task.deadline));
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(task.deadline_misses));
    }
    ImGui::EndTable();
}

void bosch_summary(const RunResult& result) {
    if (result.scenario_kind != "bosch")
        return;
    ImGui::SeparatorText("Bosch Summary");
    const auto analysis = derive_bosch_result_analysis(result);
    if (analysis.lateral_error == nullptr) {
        ImGui::TextDisabled("%s", analysis.diagnostic.value_or("Unavailable").c_str());
        return;
    }
    double maximum = 0.0;
    for (const auto& sample : analysis.lateral_error->samples)
        maximum = std::max(maximum, std::abs(gui_scalar_as_double(sample.value)));
    ImGui::Text("Threshold crossings: %zu | Maximum absolute lateral error: %.6g m | Critical "
                "sections: %zu",
                analysis.threshold_crossings.size(), maximum, analysis.critical_intervals.size());
}

} // namespace

void draw_results_view(const SimulationProgress& progress, const CompletedRunResult* completed,
                       CompletedResultFinalizationState finalization_state,
                       bool& open_visualizer, bool& open_export, ResultsViewState&) {
    if (completed == nullptr) {
        if (finalization_state == CompletedResultFinalizationState::Finalizing) {
            ImGui::TextUnformatted("Finalizing completed run...");
        } else if (finalization_state == CompletedResultFinalizationState::Failed) {
            ImGui::TextUnformatted("Completed-run analysis failed. See the workbench diagnostic.");
        } else {
            ImGui::TextUnformatted("Results are generated after the simulation finishes.");
        }
        ImGui::Text("Current tick: %lld / %lld", static_cast<long long>(progress.current_tick),
                    static_cast<long long>(progress.stop_tick));
        ImGui::Text("Run state: %s", state_name(progress.run_state));
        if (progress.run_state == GuiRunState::Paused)
            ImGui::TextDisabled("The simulation is paused; final analysis has not been built.");
    } else {
        summary(completed->result->metrics);
        timing(completed->result->metrics, completed->performance);
        responses(completed->result->metrics);
        bosch_summary(*completed->result);
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(completed == nullptr);
    if (ImGui::Button("Open Plot Visualizer..."))
        open_visualizer = true;
    ImGui::SameLine();
    if (ImGui::Button("Export Completed Results..."))
        open_export = true;
    ImGui::EndDisabled();
    if (completed == nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Available after the simulation finishes.");
}

} // namespace cpssim::gui
