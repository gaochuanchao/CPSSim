/***
 * File: tests/application/project/project_persistence_test.cpp
 * Purpose: Verify strict project persistence, safe paths, and atomic activation.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#include "cpssim/application/project/project.hpp"
#include "cpssim/application/project/project_workflow.hpp"
#include "cpssim/config/json_config.hpp"
#include "cpssim/config/json_run_plan.hpp"
#include "cpssim/gui/application_state.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace cpssim;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        const auto suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
            std::to_string(sequence.fetch_add(1));
        root_ = std::filesystem::temp_directory_path() / ("cpssim-project-test-" + suffix);
        if (!std::filesystem::create_directory(root_)) {
            throw std::runtime_error{"could not create temporary project-test directory"};
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
};

class StubFileDialog final : public FileDialog {
  public:
    FileDialogResult project_result;
    FileDialogResult results_result;

    FileDialogResult open_project(const std::filesystem::path&) override { return project_result; }
    FileDialogResult choose_project_parent(const std::filesystem::path&) override {
        return FileDialogResult::cancelled();
    }
    FileDialogResult choose_trajectory_directory(const std::filesystem::path&) override {
        return FileDialogResult::cancelled();
    }
    FileDialogResult open_run_plan(const std::filesystem::path&) override {
        return FileDialogResult::cancelled();
    }
    FileDialogResult save_run_plan(const std::filesystem::path&) override {
        return FileDialogResult::cancelled();
    }
    FileDialogResult choose_results_directory(const std::filesystem::path&) override {
        return results_result;
    }
};

ExperimentConfig make_project_config(std::string resource_name = "local") {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, std::move(resource_name)}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1}}};
}

RunPlan make_project_plan(const ExperimentConfig& config, Tick stop_tick = 100) {
    const auto result = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = stop_tick,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}});
    if (!result.valid()) {
        throw std::logic_error{"project test run plan must be valid"};
    }
    return *result.plan;
}

ProjectCreationRequest make_request(const std::filesystem::path& parent, std::string name,
                                    std::string resource_name = "local") {
    auto system = make_project_config(std::move(resource_name));
    auto plan = make_project_plan(system);
    return ProjectCreationRequest{.parent_directory = parent,
                                  .name = std::move(name),
                                  .system = std::move(system),
                                  .default_run_plan = std::move(plan),
                                  .scenario_file = std::nullopt,
                                  .scenario_kind = "generic",
                                  .workspace = {}};
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"could not read project test file"};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void write_text(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"could not write project test file"};
    }
    output << contents;
}

std::string project_document(std::string_view system_file, std::string_view workspace_file,
                             std::string_view run_plan_file, unsigned int schema_version = 1) {
    std::ostringstream document;
    document << "{\n"
             << "  \"schema_version\": " << schema_version << ",\n"
             << "  \"name\": \"example-project\",\n"
             << "  \"system_file\": \"" << system_file << "\",\n"
             << "  \"workspace_file\": \"" << workspace_file << "\",\n"
             << "  \"default_run_plan\": \"" << run_plan_file << "\",\n"
             << "  \"scenario\": {\"kind\": \"generic\"}\n"
             << "}\n";
    return document.str();
}

TEST_CASE("result-directory dialog preserves selection and cancellation", "[project][dialog]") {
    StubFileDialog dialogs;
    dialogs.results_result = FileDialogResult::cancelled();
    REQUIRE(dialogs.choose_results_directory("projects").status == FileDialogStatus::Cancelled);
    dialogs.results_result = FileDialogResult::selected("exports");
    const auto selected = dialogs.choose_results_directory("projects");
    REQUIRE(selected.status == FileDialogStatus::Selected);
    REQUIRE(selected.path == std::filesystem::path{"exports"});
}

TEST_CASE("project creation writes and reloads the complete directory convention",
          "[project][persistence][round-trip]") {
    TemporaryDirectory temporary;
    const auto request = make_request(temporary.root(), "example-project");
    const auto expected_system = serialize_experiment_config_json(request.system);
    const auto expected_plan = request.default_run_plan;

    const auto created = create_project(request);
    const auto root = temporary.root() / "example-project";
    REQUIRE((created->root() == std::filesystem::weakly_canonical(root)));
    REQUIRE(std::filesystem::is_regular_file(root / "project.json"));
    REQUIRE(std::filesystem::is_regular_file(root / "system.json"));
    REQUIRE(std::filesystem::is_regular_file(root / "run-plans" / "default.json"));
    REQUIRE(std::filesystem::is_regular_file(root / "workspace.json"));
    REQUIRE(std::filesystem::is_directory(root / "results"));

    const auto metadata = parse_project_metadata_json(read_text(root / "project.json"));
    REQUIRE((metadata == created->metadata()));
    REQUIRE(metadata.system_file.is_relative());
    REQUIRE(metadata.workspace_file.is_relative());
    REQUIRE(metadata.default_run_plan.is_relative());

    const auto loaded = load_project(root / "project.json");
    REQUIRE((loaded->metadata() == metadata));
    REQUIRE((loaded->workspace() == ProjectWorkspace{}));
    REQUIRE((serialize_experiment_config_json(loaded->session().config()) == expected_system));
    REQUIRE((loaded->default_run_plan() == expected_plan));
    REQUIRE((loaded->session().snapshot().run_state == GuiRunState::Paused));
}

TEST_CASE("project metadata and presentation workspace JSON round trip strictly",
          "[project][persistence][json]") {
    const ProjectMetadata metadata{.name = "round-trip",
                                   .system_file = "definitions/system.json",
                                   .workspace_file = "ui/workspace.json",
                                   .default_run_plan = "plans/main.json",
                                   .scenario_file = std::nullopt,
                                   .scenario_kind = "generic"};

    REQUIRE((parse_project_metadata_json(serialize_project_metadata_json(metadata)) == metadata));
    REQUIRE((parse_project_workspace_json(serialize_project_workspace_json(ProjectWorkspace{})) ==
             ProjectWorkspace{}));
    auto workspace = ProjectWorkspace{};
    workspace.theme = GuiTheme::Light;
    workspace.panels.events = false;
    workspace.center_split_ratio = 0.7F;
    REQUIRE(move_center_tab(workspace, GuiCenterTab::Signals, false));
    workspace.active_upper_tab = GuiCenterTab::Results;
    workspace.run_mode = GuiRunMode::Fast;
    workspace.fast_batch_unit = GuiFastBatchUnit::Ticks;
    workspace.fast_event_batch_size = 17;
    workspace.fast_tick_batch_size = 29;
    workspace.plot_x_axis_unit = GuiPlotXAxisUnit::Seconds;
    workspace.plot_range_mode = GuiPlotRangeMode::Custom;
    workspace.plot_custom_begin = 5;
    workspace.plot_custom_end = 19;
    workspace.plot_auto_y = false;
    workspace.plot_y_min = -0.5;
    workspace.plot_y_max = 0.75;
    workspace.plot_grid = false;
    workspace.plot_legend = false;
    workspace.plot_line_thickness = 3.0F;
    workspace.plot_markers = true;
    workspace.plot_bosch_thresholds = false;
    workspace.plot_bosch_critical_sections = false;
    workspace.plot_bosch_deadline_misses = false;
    workspace.plot_selected_tick = false;
    workspace.event_filters.type = EventType::JobFinish;
    workspace.event_filters.task = TaskId{1};
    workspace.event_filters.text = "finished";
    workspace.event_columns.phase = false;
    workspace.selected_signals = {{GuiSignalScalarType::Real, "vehicle.x"}};
    workspace.architecture.mode = GuiArchitectureMode::Arrange;
    workspace.architecture.pan_x = 17.0F;
    workspace.architecture.pan_y = -9.0F;
    workspace.architecture.zoom = 1.4F;
    set_task_layout_position(workspace.architecture, TaskId{1}, {25.0F, 30.0F});
    set_resource_layout_position(workspace.architecture, ResourceId{1}, {10.0F, 15.0F});
    set_resource_layout_size(workspace.architecture, ResourceId{1}, {260.0F, 180.0F});
    workspace.results_summary_ratio = 0.34F;
    workspace.results_timing_ratio = 0.47F;
    REQUIRE(
        (parse_project_workspace_json(serialize_project_workspace_json(workspace)) == workspace));
    REQUIRE((parse_project_workspace_json(R"({"schema_version":1})") == ProjectWorkspace{}));
    REQUIRE((parse_project_workspace_json(R"({"schema_version":4})") == ProjectWorkspace{}));
    REQUIRE_THROWS_AS(parse_project_workspace_json(R"({"schema_version":99})"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_project_workspace_json(R"({"schema_version":2,"unknown":true})"),
                      std::invalid_argument);
}

TEST_CASE("invalid workspace values use safe presentation defaults", "[project][workspace]") {
    const auto parsed = parse_project_workspace_json(
        R"({"schema_version":2,"theme":"invalid","splitters":{"analysis_lower":-4.0,"right_sidebar":8.0},"active_tabs":{"analysis":"invalid","resources":"invalid"}})");
    REQUIRE(parsed.theme == GuiTheme::Dark);
    REQUIRE(parsed.center_split_ratio == 0.05F);
    REQUIRE(parsed.right_sidebar_ratio == 0.95F);
    REQUIRE(parsed.active_upper_tab == GuiCenterTab::Architecture);
}

TEST_CASE("project loading resolves valid nested relative references",
          "[project][persistence][paths]") {
    TemporaryDirectory temporary;
    auto created = create_project(make_request(temporary.root(), "nested", "nested-resource"));
    const auto root = created->root();
    std::filesystem::create_directories(root / "definitions");
    std::filesystem::create_directories(root / "plans");
    std::filesystem::create_directories(root / "ui");
    std::filesystem::rename(root / "system.json", root / "definitions" / "system.json");
    std::filesystem::rename(root / "run-plans" / "default.json", root / "plans" / "main.json");
    std::filesystem::rename(root / "workspace.json", root / "ui" / "workspace.json");

    const ProjectMetadata metadata{.name = "nested",
                                   .system_file = "definitions/system.json",
                                   .workspace_file = "ui/workspace.json",
                                   .default_run_plan = "plans/main.json",
                                   .scenario_file = std::nullopt,
                                   .scenario_kind = "generic"};
    write_text(root / "project.json", serialize_project_metadata_json(metadata));

    const auto loaded = load_project(root / "project.json");
    REQUIRE((loaded->metadata() == metadata));
    REQUIRE((loaded->session().config().resources()[0].name() == "nested-resource"));
}

TEST_CASE("project loading rejects absolute and escaping internal paths",
          "[project][persistence][paths][invalid]") {
    TemporaryDirectory temporary;
    auto created = create_project(make_request(temporary.root(), "unsafe"));
    const auto project_file = created->root() / "project.json";

    write_text(project_file,
               project_document("/tmp/system.json", "workspace.json", "run-plans/default.json"));
    REQUIRE_THROWS_AS(load_project(project_file), std::invalid_argument);

    write_text(project_file,
               project_document("../system.json", "workspace.json", "run-plans/default.json"));
    REQUIRE_THROWS_AS(load_project(project_file), std::invalid_argument);
}

TEST_CASE("project loading reports missing metadata and referenced files",
          "[project][persistence][missing]") {
    TemporaryDirectory temporary;
    auto created = create_project(make_request(temporary.root(), "missing"));
    const auto project_file = created->root() / "project.json";

    std::filesystem::remove(created->root() / "system.json");
    REQUIRE_THROWS_WITH(load_project(project_file),
                        Catch::Matchers::ContainsSubstring("system_file"));

    write_text(project_file, R"({
      "schema_version": 1,
      "name": "missing",
      "system_file": "system.json",
      "default_run_plan": "run-plans/default.json",
      "scenario": {"kind": "generic"}
    })");
    REQUIRE_THROWS_WITH(load_project(project_file),
                        Catch::Matchers::ContainsSubstring("workspace_file"));
}

TEST_CASE("project loading rejects unsupported schemas and malformed JSON",
          "[project][persistence][invalid]") {
    TemporaryDirectory temporary;
    auto created = create_project(make_request(temporary.root(), "invalid-json"));
    const auto project_file = created->root() / "project.json";

    write_text(project_file,
               project_document("system.json", "workspace.json", "run-plans/default.json", 2));
    REQUIRE_THROWS_AS(load_project(project_file), std::invalid_argument);

    write_text(project_file, "{");
    REQUIRE_THROWS_AS(load_project(project_file), std::invalid_argument);
}

TEST_CASE("invalid project run plans fail before application replacement",
          "[project][persistence][atomic]") {
    TemporaryDirectory temporary;
    auto retained = create_project(make_request(temporary.root(), "retained", "retained"));
    auto invalid = create_project(make_request(temporary.root(), "invalid", "invalid"));
    const auto invalid_project_file = invalid->root() / "project.json";
    write_text(invalid->root() / "run-plans" / "default.json", R"({"schema_version":1})");

    GuiApplicationState application_state;
    application_state.replace_project(std::move(retained));
    const auto* retained_session = &application_state.active_session();

    REQUIRE_THROWS_AS(application_state.replace_project(load_project(invalid_project_file)),
                      std::invalid_argument);
    REQUIRE((application_state.screen() == GuiApplicationScreen::Workbench));
    REQUIRE(application_state.has_active_project());
    REQUIRE((&application_state.active_session() == retained_session));
    REQUIRE((application_state.active_project().metadata().name == "retained"));
}

TEST_CASE("saving a running project excludes runtime and canonical trace state",
          "[project][persistence][runtime]") {
    TemporaryDirectory temporary;
    auto project = create_project(make_request(temporary.root(), "runtime"));
    REQUIRE(project->session().enqueue(GuiCommand::StepNextEvent));
    project->session().update();
    REQUIRE_FALSE(project->session().snapshot().event_log.empty());

    save_project(*project);
    const auto root = project->root();
    const auto persisted = read_text(root / "project.json") + read_text(root / "system.json") +
                           read_text(root / "run-plans" / "default.json") +
                           read_text(root / "workspace.json");
    REQUIRE((persisted.find("current_tick") == std::string::npos));
    REQUIRE((persisted.find("canonical_events") == std::string::npos));
    REQUIRE((persisted.find("event_log") == std::string::npos));
    REQUIRE((persisted.find("active_jobs") == std::string::npos));

    const auto reloaded = load_project(root / "project.json");
    REQUIRE(reloaded->session().snapshot().event_log.empty());
    REQUIRE((reloaded->session().snapshot().run_state == GuiRunState::Paused));
}

TEST_CASE("project creation validates names and never reuses an existing root",
          "[project][persistence][create]") {
    TemporaryDirectory temporary;
    REQUIRE_THROWS_AS(create_project(make_request(temporary.root(), "../escape")),
                      std::invalid_argument);

    auto created = create_project(make_request(temporary.root(), "existing"));
    REQUIRE_THROWS_AS(create_project(make_request(temporary.root(), "existing")),
                      std::runtime_error);
    REQUIRE(std::filesystem::is_regular_file(created->root() / "project.json"));
}

TEST_CASE("dialog cancellation and invalid selection preserve the active project",
          "[project][dialog][atomic]") {
    TemporaryDirectory temporary;
    auto retained = create_project(make_request(temporary.root(), "retained-dialog"));
    GuiApplicationState state{std::move(retained)};
    const auto* retained_session = &state.active_session();
    StubFileDialog dialogs;

    dialogs.project_result = FileDialogResult::cancelled();
    REQUIRE((open_project_from_dialog(state, dialogs, temporary.root()).status ==
             ProjectWorkflowStatus::Cancelled));
    REQUIRE((&state.active_session() == retained_session));

    dialogs.project_result = FileDialogResult::selected(temporary.root() / "missing.json");
    const auto invalid = open_project_from_dialog(state, dialogs, temporary.root());
    REQUIRE((invalid.status == ProjectWorkflowStatus::Failed));
    REQUIRE_FALSE(invalid.diagnostic.empty());
    REQUIRE((&state.active_session() == retained_session));

    auto replacement = create_project(make_request(temporary.root(), "selected-dialog"));
    dialogs.project_result = FileDialogResult::selected(replacement->root() / "project.json");
    REQUIRE((open_project_from_dialog(state, dialogs, temporary.root()).status ==
             ProjectWorkflowStatus::Applied));
    REQUIRE((state.active_project().metadata().name == "selected-dialog"));
}

TEST_CASE("Save Project As validates the copy before returning a replacement",
          "[project][save-as][atomic]") {
    TemporaryDirectory temporary;
    auto source = create_project(make_request(temporary.root(), "source"));
    auto copy = save_project_as(*source, temporary.root(), "copy");

    REQUIRE((copy->metadata().name == "copy"));
    REQUIRE((copy->root() == std::filesystem::weakly_canonical(temporary.root() / "copy")));
    REQUIRE(std::filesystem::is_regular_file(copy->root() / "project.json"));
    REQUIRE((copy->session().snapshot().run_state == GuiRunState::Paused));
    REQUIRE((source->metadata().name == "source"));
}

TEST_CASE("project save and Save As persist the accepted run plan", "[project][save-as][plan]") {
    TemporaryDirectory temporary;
    auto source = create_project(make_request(temporary.root(), "accepted-plan"));
    REQUIRE(source->session().set_draft_stop_tick(321));
    REQUIRE(source->session().apply_draft());

    save_project(*source);
    const auto reloaded = load_project(source->root() / "project.json");
    REQUIRE(reloaded->default_run_plan().stop_tick() == 321);

    auto copy = save_project_as(*source, temporary.root(), "accepted-plan-copy");
    REQUIRE(copy->default_run_plan().stop_tick() == 321);
}

TEST_CASE("failed project content preparation removes the incomplete directory",
          "[project][create][cleanup]") {
    TemporaryDirectory temporary;
    const auto request = make_request(temporary.root(), "will-fail");
    REQUIRE_THROWS_AS(
        create_project(request, {},
                       [](const auto&) { throw std::runtime_error{"injected content failure"}; }),
        std::runtime_error);
    REQUIRE_FALSE(std::filesystem::exists(temporary.root() / "will-fail"));
}

TEST_CASE("failed Save As content preparation removes the incomplete copy",
          "[project][save-as][cleanup]") {
    TemporaryDirectory temporary;
    const auto source = create_project(make_request(temporary.root(), "copy-source"));
    REQUIRE_THROWS_AS(
        save_project_as(*source, temporary.root(), "copy-will-fail", {},
                        [](const auto&) { throw std::runtime_error{"injected content failure"}; }),
        std::runtime_error);
    REQUIRE_FALSE(std::filesystem::exists(temporary.root() / "copy-will-fail"));
}

TEST_CASE("invalid optional workspace uses defaults without changing simulation input",
          "[project][workspace][fallback]") {
    TemporaryDirectory temporary;
    auto created = create_project(make_request(temporary.root(), "workspace-fallback"));
    const auto expected_system = serialize_experiment_config_json(created->session().config());
    write_text(created->root() / "workspace.json", R"({"schema_version":99})");

    const auto loaded = load_project(created->root() / "project.json");
    REQUIRE((loaded->workspace() == ProjectWorkspace{}));
    REQUIRE(loaded->workspace_diagnostic().has_value());
    REQUIRE((serialize_experiment_config_json(loaded->session().config()) == expected_system));
}

TEST_CASE("presentation workspace save does not alter semantic project files",
          "[project][workspace][semantics]") {
    TemporaryDirectory temporary;
    auto project = create_project(make_request(temporary.root(), "presentation-only"));
    const auto system_before = read_text(project->root() / "system.json");
    const auto plan_before = read_text(project->root() / "run-plans" / "default.json");

    auto workspace = project->workspace();
    workspace.theme = GuiTheme::Light;
    workspace.panels.events = false;
    workspace.center_split_ratio = 0.75F;
    workspace.event_filters.text = "deadline";
    project->set_workspace(workspace);
    save_project(*project);

    REQUIRE(read_text(project->root() / "system.json") == system_before);
    REQUIRE(read_text(project->root() / "run-plans" / "default.json") == plan_before);
    const auto reloaded = load_project(project->root() / "project.json");
    REQUIRE(reloaded->workspace().theme == GuiTheme::Light);
    REQUIRE_FALSE(reloaded->workspace().panels.events);
    REQUIRE(reloaded->workspace().event_filters.text == "deadline");
}

} // namespace
