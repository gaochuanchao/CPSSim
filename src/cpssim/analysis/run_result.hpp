/*** Immutable, graphics-independent run results and deterministic derived metrics. ***/

#pragma once

#include "cpssim/gui/simulation_controller.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

/*** Exact integer-tick statistics. Mean is presentation-only and never canonical time. ***/
struct TickStatistics {
    std::uint64_t count{};
    Tick minimum{};
    Tick maximum{};
    Tick total{};

    double mean() const noexcept;
    bool operator==(const TickStatistics&) const = default;
};

struct TaskResponseMetrics {
    TaskId task_id;
    std::string task_name;
    Tick deadline{};
    std::uint64_t completed_jobs{};
    std::uint64_t deadline_misses{};
    std::optional<TickStatistics> response_time;

    bool operator==(const TaskResponseMetrics&) const = default;
};

struct ResourceRunMetrics {
    ResourceId resource_id;
    std::string resource_name;
    Tick busy_ticks{};
    Tick idle_ticks{};
    std::optional<double> utilization;

    bool operator==(const ResourceRunMetrics&) const = default;
};

struct MessageRunMetrics {
    std::uint64_t sent{};
    std::uint64_t delivered{};
    std::optional<TickStatistics> delivery_delay;

    bool operator==(const MessageRunMetrics&) const = default;
};

struct RunMetrics {
    std::uint64_t event_count{};
    PhysicalDuration tick_period{};
    Tick horizon_tick{};
    std::optional<PhysicalDuration> horizon_time;
    std::uint64_t completed_jobs{};
    std::uint64_t deadline_misses{};
    std::uint64_t preemptions{};
    std::vector<TaskResponseMetrics> task_responses;
    std::vector<ResourceRunMetrics> resources;
    MessageRunMetrics messages;

    bool operator==(const RunMetrics&) const = default;
};

/*** Owns one immutable detached run used identically by views and exporters. ***/
struct RunResult {
    std::string scenario_kind;
    SimulationSnapshot snapshot;
    GuiSignalBuildResult signals;
    RunMetrics metrics;
};

RunMetrics derive_run_metrics(const SimulationSnapshot& snapshot);
RunResult build_run_result(SimulationSnapshot snapshot, std::string scenario_kind);

std::string_view canonical_event_type_name(EventType type);
std::string_view canonical_event_phase_name(EventPhase phase);

/*** Returns a snapshot copy limited to one inclusive logical-tick range. ***/
SimulationSnapshot select_run_result_range(const SimulationSnapshot& snapshot, Tick begin_tick,
                                           Tick end_tick);

} // namespace cpssim
