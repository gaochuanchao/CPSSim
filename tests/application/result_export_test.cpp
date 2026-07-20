/*** Verify deterministic result schemas and atomic raw export publication. ***/

#include "cpssim/application/project/project_template.hpp"
#include "cpssim/application/result_export.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using namespace cpssim;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        root_ = std::filesystem::temp_directory_path() /
                ("cpssim-result-export-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
                 std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directory(root_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
};

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

EventEntityRefs task_job(TaskId task, JobId job) {
    EventEntityRefs refs;
    refs.task_id = task;
    refs.job_id = job;
    return refs;
}

RunResult result_for(const ProjectContext& project) {
    auto snapshot = project.session().snapshot();
    snapshot.current_tick = 9'007'199'254'740'995LL;
    snapshot.event_log = {Event{1, EventPhase::JobRelease, EventSequence{0}, EventType::JobRelease,
                                task_job(TaskId{1}, JobId{0})},
                          Event{3, EventPhase::ExecutionCompletion, EventSequence{1},
                                EventType::JobFinish, task_job(TaskId{1}, JobId{0})}};
    snapshot.functional_signal_registry = {{.id = {GuiSignalScalarType::Real, "value"},
                                            .path = "Generic/Value",
                                            .display_name = "Value, quoted \"name\"",
                                            .unit = "m",
                                            .source = "test"}};
    snapshot.functional_observations = {{.tick = 0,
                                         .real_signals = {{.name = "value", .value = 1.25}},
                                         .integer_signals = {},
                                         .boolean_signals = {}},
                                        {.tick = 1,
                                         .real_signals = {{.name = "value", .value = 2.5}},
                                         .integer_signals = {},
                                         .boolean_signals = {}}};
    return build_run_result(std::move(snapshot), "generic");
}

TEST_CASE("run manifest round trips scenario provenance", "[project][result][manifest]") {
    const RunManifest expected{.schema_version = 1,
                               .cpssim_version = "0.1.0",
                               .project_name = "project",
                               .run_id = "run-001",
                               .created_at_utc = "2026-07-20T10:00:00Z",
                               .system_file = "system.json",
                               .run_plan_file = "run-plan.json",
                               .system_checksum = "fnv1a64:1234",
                               .run_plan_checksum = "fnv1a64:5678",
                               .policy = "fixed_priority",
                               .stop_tick = 42,
                               .scenario_kind = "bosch",
                               .scenario = {.bosch_trajectory = "example_v_10",
                                            .fmu_identity = "LateralMotionControl",
                                            .fmu_path = "LateralMotionControl.so"}};
    REQUIRE(parse_run_manifest_json(serialize_run_manifest_json(expected)) == expected);
    REQUIRE_THROWS_AS(parse_run_manifest_json("{"), std::invalid_argument);
}

TEST_CASE("raw export publishes complete stable files and rejects duplicate run IDs",
          "[project][result][export]") {
    TemporaryDirectory temporary;
    auto project = create_project(make_generic_project_template(temporary.root(), "export"));
    const auto result = result_for(*project);
    const RunExportOptions options{.destination_directory = project->root() / "results",
                                   .run_id = "run-001",
                                   .scope = RunExportScope::Complete,
                                   .selected_range = std::nullopt,
                                   .include_excel = false,
                                   .scenario = {},
                                   .control_metrics = {},
                                   .created_at_utc = "2026-07-20T10:00:00Z"};
    const auto exported = export_run_result(*project, result, options);

    REQUIRE(exported.event_rows == 2);
    REQUIRE(exported.signal_rows == 2);
    for (const auto* name : {"manifest.json", "system.json", "run-plan.json", "events.jsonl",
                             "events.csv", "signals.csv", "metrics.json", "metrics.csv"}) {
        REQUIRE(std::filesystem::is_regular_file(exported.run_directory / name));
    }
    REQUIRE(read_text(exported.run_directory / "metrics.json").find("9007199254740995") !=
            std::string::npos);
    REQUIRE(
        read_text(exported.run_directory / "signals.csv").find("\"Value, quoted \"\"name\"\"\"") !=
        std::string::npos);
    REQUIRE(read_text(exported.run_directory / "events.jsonl").find("\"sequence\":0") !=
            std::string::npos);
    REQUIRE(exported.manifest.system_checksum.starts_with("fnv1a64:"));
    REQUIRE_THROWS_AS(export_run_result(*project, result, options), std::invalid_argument);
}

TEST_CASE("selected ranges are inclusive and failed export removes its temporary directory",
          "[project][result][atomic]") {
    TemporaryDirectory temporary;
    auto project = create_project(make_generic_project_template(temporary.root(), "range"));
    auto result = result_for(*project);
    auto options = RunExportOptions{.destination_directory = project->root() / "results",
                                    .run_id = "selected",
                                    .scope = RunExportScope::SelectedRange,
                                    .selected_range = GuiTickRange{1, 1},
                                    .include_excel = false,
                                    .scenario = {},
                                    .control_metrics = {},
                                    .created_at_utc = "2026-07-20T10:00:00Z"};
    const auto selected = export_run_result(*project, result, options);
    REQUIRE(selected.event_rows == 1);
    REQUIRE(selected.signal_rows == 1);

    result.signals.model.reset();
    result.signals.diagnostics.push_back({.code = GuiSignalDiagnosticCode::InvalidSignal,
                                          .observation_index = 0,
                                          .tick = 0,
                                          .signal_id = std::nullopt,
                                          .message = "forced failure"});
    options.run_id = "failed";
    REQUIRE_THROWS_AS(export_run_result(*project, result, options), std::invalid_argument);
    REQUIRE_FALSE(std::filesystem::exists(options.destination_directory / options.run_id));
    for (const auto& entry : std::filesystem::directory_iterator(options.destination_directory)) {
        REQUIRE(entry.path().filename().string().find(".failed.tmp-") == std::string::npos);
    }
}

TEST_CASE("Excel export creates a workbook and large detail tables split deterministically",
          "[project][result][excel]") {
    const auto empty = plan_workbook_detail_sheets("Events", 0);
    REQUIRE(empty == std::vector<WorkbookDetailSheet>{{"Events", 0, 0}});
    const auto split = plan_workbook_detail_sheets("Events", excel_maximum_rows + 5);
    REQUIRE(split == std::vector<WorkbookDetailSheet>{{"Events", 0, excel_maximum_rows - 1},
                                                      {"Events 2", excel_maximum_rows - 1, 6}});

    TemporaryDirectory temporary;
    auto project = create_project(make_generic_project_template(temporary.root(), "excel"));
    const auto result = result_for(*project);
    const RunExportOptions options{
        .destination_directory = project->root() / "results",
        .run_id = "workbook",
        .scope = RunExportScope::Complete,
        .selected_range = std::nullopt,
        .include_excel = true,
        .scenario = {},
        .control_metrics = {{.metric = "threshold", .tick = 3, .value = "0.2 m"}},
        .created_at_utc = "2026-07-20T10:00:00Z"};
    const auto exported = export_run_result(*project, result, options);
    const auto workbook = read_text(exported.run_directory / "results.xlsx");
    REQUIRE(workbook.size() > 4);
    REQUIRE(workbook.substr(0, 2) == "PK");
    REQUIRE(exported.event_rows == 2);
    REQUIRE(exported.signal_rows == 2);
}

} // namespace
