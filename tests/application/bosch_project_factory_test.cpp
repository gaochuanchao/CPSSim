/*** Verify GUI Bosch project construction without running the whole simulation. ***/

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/project/system_builder_workflow.hpp"
#include "cpssim/application/project/system_edit_policy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        root_ = std::filesystem::temp_directory_path() /
                ("cpssim-bosch-project-test-" +
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

std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
}

std::filesystem::path fmu_library() {
    const char* value = std::getenv("CPSSIM_G1_BOSCH_FMU_LIBRARY");
    if (value == nullptr || std::string_view{value}.empty()) {
        throw std::runtime_error{"CPSSIM_G1_BOSCH_FMU_LIBRARY is not set"};
    }
    return value;
}

void write_values(const std::filesystem::path& path, std::string_view values) {
    std::ofstream output{path};
    output << values;
}

std::filesystem::path make_trajectory(const std::filesystem::path& parent, std::string_view name) {
    const auto root = parent / name;
    std::filesystem::create_directory(root);
    write_values(root / "time_vector.csv", "0\n0.0001\n0.0002\n");
    write_values(root / "feedforward_sequence_0.csv", "0\n0\n0\n");
    write_values(root / "feedforward_sequence_1.csv", "0\n0\n0\n");
    write_values(root / "velocity.csv", "10\n10\n10\n");
    write_values(root / "x_position_track.csv", "0\n0\n0\n");
    write_values(root / "y_position_track.csv", "0\n0\n0\n");
    return root;
}

} // namespace

TEST_CASE("Bosch factory accepts every wizard trajectory choice and both scenarios paused",
          "[project][bosch][factory]") {
    TemporaryDirectory temporary;
    constexpr std::array<std::string_view, 3> names{"example_v_10", "example_v_12_5",
                                                    "example_v_15"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        CAPTURE(names[index]);
        const auto trajectory = make_trajectory(temporary.root(), names[index]);
        const auto scenario = index == 1 ? cpssim::BoschReferenceScenario::SharedCloud
                                         : cpssim::BoschReferenceScenario::Dedicated;
        auto project = cpssim::create_bosch_project(
            {.parent_directory = temporary.root(),
             .name = "project-" + std::to_string(index),
             .trajectory_directory = trajectory,
             .scenario = scenario,
             .stop_tick = 2,
             .reference_root = repository_root() / "experiments/bosch_v10_reference",
             .shared_library = fmu_library()});
        REQUIRE((project->metadata().scenario_kind == "bosch"));
        REQUIRE(project->metadata().scenario_file.has_value());
        REQUIRE((project->session().snapshot().run_state == cpssim::GuiRunState::Paused));
        REQUIRE(project->session().snapshot().functional_model_attached);
        REQUIRE(std::filesystem::is_directory(project->root() / "trajectory"));

        const auto resolver = [](const auto& root, const auto& metadata) {
            return cpssim::resolve_bosch_project_runtime(
                root, metadata, repository_root() / "experiments/bosch_v10_reference",
                fmu_library());
        };
        const auto reopened = cpssim::load_project(project->root() / "project.json", resolver);
        REQUIRE((reopened->session().snapshot().run_state == cpssim::GuiRunState::Paused));
    }
}

TEST_CASE("Bosch request validation rejects bad bounds before leaving a project directory",
          "[project][bosch][validation][cleanup]") {
    TemporaryDirectory temporary;
    const auto trajectory = make_trajectory(temporary.root(), "trajectory");
    const cpssim::BoschProjectRequest request{.parent_directory = temporary.root(),
                                              .name = "invalid-horizon",
                                              .trajectory_directory = trajectory,
                                              .scenario = cpssim::BoschReferenceScenario::Dedicated,
                                              .stop_tick = 3,
                                              .reference_root = repository_root() /
                                                                "experiments/bosch_v10_reference",
                                              .shared_library = fmu_library()};
    REQUIRE_THROWS_AS(cpssim::create_bosch_project(request), std::invalid_argument);
    REQUIRE_FALSE(std::filesystem::exists(temporary.root() / request.name));

    auto missing = request;
    missing.name = "missing-trajectory";
    missing.trajectory_directory = temporary.root() / "absent";
    REQUIRE_THROWS_AS(cpssim::validate_bosch_project_request(missing), std::invalid_argument);
    REQUIRE_FALSE(std::filesystem::exists(temporary.root() / missing.name));
}

TEST_CASE("bundled FMU resolution stays beside the GUI executable", "[project][bosch][fmu]") {
    const auto library = fmu_library();
    const auto executable = library.parent_path() / "cpssim_gui";
    REQUIRE((cpssim::resolve_bundled_bosch_fmu(executable) == library));
}

TEST_CASE("Bosch edit policy protects FMU identities while allowing timing changes",
          "[project][bosch][editing]") {
    TemporaryDirectory temporary;
    const auto trajectory = make_trajectory(temporary.root(), "editable-trajectory");
    auto project = cpssim::create_bosch_project(
        {.parent_directory = temporary.root(),
         .name = "editable",
         .trajectory_directory = trajectory,
         .scenario = cpssim::BoschReferenceScenario::Dedicated,
         .stop_tick = 2,
         .reference_root = repository_root() / "experiments/bosch_v10_reference",
         .shared_library = fmu_library()});
    REQUIRE(cpssim::project_system_edit_policy(project->metadata()) ==
            cpssim::ProjectSystemEditPolicy::BoschCompatible);
    REQUIRE(cpssim::bosch_experiment_status(*project) ==
            cpssim::BoschExperimentStatus::ReferenceBaseline);
    cpssim::EditableSystemDraft draft{project->session().config()};
    const auto added_resource = draft.add_resource();
    draft.set_resource_name(draft.resources().size() - 1, "Experimental cloud");
    draft.set_execution_profile(cpssim::TaskId{3}, added_resource, 2);
    const auto timing = draft.tasks()[0];
    draft.set_task_timing(
        0, {.period = timing.period + 1, .deadline = timing.deadline, .offset = timing.offset},
        timing.priority);
    auto delayed_route = draft.routes().front();
    ++delayed_route.delay;
    draft.set_message_route(0, delayed_route);
    const auto replacement = cpssim::build_system_project_replacement(*project, draft);
    REQUIRE(replacement.valid());
    REQUIRE(cpssim::bosch_experiment_status(*replacement.replacement) ==
            cpssim::BoschExperimentStatus::Modified);
    cpssim::save_project(*replacement.replacement);
    const auto reopened = cpssim::load_project(
        replacement.replacement->root() / "project.json",
        [](const auto& root, const auto& metadata) {
            return cpssim::resolve_bosch_project_runtime(
                root, metadata, repository_root() / "experiments/bosch_v10_reference",
                fmu_library());
        });
    REQUIRE(cpssim::bosch_experiment_status(*reopened) == cpssim::BoschExperimentStatus::Modified);

    cpssim::EditableSystemDraft corrupted{reopened->session().config()};
    corrupted.set_task_name(0, "Not Sensor");
    REQUIRE_FALSE(cpssim::build_system_project_replacement(*reopened, corrupted).valid());

    corrupted = cpssim::EditableSystemDraft{reopened->session().config()};
    static_cast<void>(corrupted.add_task());
    REQUIRE_FALSE(cpssim::build_system_project_replacement(*reopened, corrupted).valid());

    corrupted = cpssim::EditableSystemDraft{reopened->session().config()};
    auto changed_endpoint = corrupted.routes().front();
    changed_endpoint.destination_task_id = cpssim::TaskId{3};
    corrupted.set_message_route(0, changed_endpoint);
    REQUIRE_FALSE(cpssim::build_system_project_replacement(*reopened, corrupted).valid());
}

TEST_CASE("Bosch functional dependencies are presentation-only and deterministic",
          "[project][bosch][architecture]") {
    const auto dependencies = cpssim::bosch_functional_dependencies();
    REQUIRE(dependencies == std::vector<cpssim::GuiFunctionalDependency>{
                                {cpssim::TaskId{2}, cpssim::TaskId{3}, "Estimator to Controller"},
                                {cpssim::TaskId{3}, cpssim::TaskId{5}, "Controller to Merger"},
                                {cpssim::TaskId{4}, cpssim::TaskId{5}, "Feedforward to Merger"}});
}
