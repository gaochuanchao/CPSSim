/*** Atomic, graphics-independent persistence for one immutable run result. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/application/project/project.hpp"
#include "cpssim/application/results_workbook.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace cpssim {

inline constexpr std::uint32_t current_run_manifest_schema_version = 1;

struct RunScenarioMetadata {
    std::optional<std::string> bosch_trajectory;
    std::optional<std::string> fmu_identity;
    std::optional<std::filesystem::path> fmu_path;

    bool operator==(const RunScenarioMetadata&) const = default;
};

struct RunManifest {
    std::uint32_t schema_version{current_run_manifest_schema_version};
    std::string cpssim_version;
    std::string project_name;
    std::string run_id;
    std::string created_at_utc;
    std::string system_file{"system.json"};
    std::string run_plan_file{"run-plan.json"};
    std::string system_checksum;
    std::string run_plan_checksum;
    std::string policy;
    Tick stop_tick{};
    std::string scenario_kind;
    RunScenarioMetadata scenario;

    bool operator==(const RunManifest&) const = default;
};

enum class RunExportScope { Complete, SelectedRange };

struct RunExportOptions {
    std::filesystem::path destination_directory;
    std::string run_id;
    RunExportScope scope{RunExportScope::Complete};
    std::optional<GuiTickRange> selected_range;
    bool include_excel{false};
    RunScenarioMetadata scenario;
    std::vector<WorkbookControlMetric> control_metrics;
    // Empty means the exporter records the current UTC time.
    std::string created_at_utc;
};

struct RunExportArtifacts {
    std::filesystem::path run_directory;
    RunManifest manifest;
    std::uint64_t event_rows{};
    std::uint64_t signal_rows{};
};

std::string serialize_run_manifest_json(const RunManifest& manifest);
RunManifest parse_run_manifest_json(std::string_view json_text);

std::string serialize_run_metrics_json(const RunMetrics& metrics);
std::string serialize_run_metrics_csv(const RunMetrics& metrics);
std::string serialize_events_csv(const SimulationSnapshot& snapshot,
                                 std::optional<GuiTickRange> range = std::nullopt);
std::string serialize_signals_csv(const RunResult& result,
                                  std::optional<GuiTickRange> range = std::nullopt);

/*** Writes all artifacts to a temporary sibling and publishes by one rename. ***/
RunExportArtifacts export_run_result(const ProjectContext& project, const RunResult& result,
                                     const RunExportOptions& options);

} // namespace cpssim
