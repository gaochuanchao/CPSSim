/***
 * File: tests/bosch/example_data_test.cpp
 * Purpose: Verify strict loading and real-FMU execution of every supplied
 *          Bosch example trajectory.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: CTest supplies the build-tree Bosch Linux shared-library path.
 ***/

#include "cpssim/application/bosch_run_service.hpp"
#include "cpssim/bosch/example_data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace {

std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
}

std::filesystem::path fmu_library() {
    const char* path = std::getenv("CPSSIM_T16_BOSCH_FMU_LIBRARY");
    if (path == nullptr || std::string_view{path}.empty()) {
        throw std::runtime_error{"CPSSIM_T16_BOSCH_FMU_LIBRARY is not set"};
    }
    return path;
}

struct ExampleExpectation {
    std::string_view directory;
    double initial_velocity;
};

} // namespace

TEST_CASE("all supplied Bosch examples load with an exact integer-tick time base",
          "[functional][bosch][examples]") {
    constexpr std::array examples{
        ExampleExpectation{"example_v_10", 10.0},
        ExampleExpectation{"example_v_12_5", 12.5},
        ExampleExpectation{"example_v_15", 15.0},
    };

    for (const auto& example : examples) {
        CAPTURE(example.directory);
        const auto trajectory = cpssim::load_bosch_example_trajectory(
            repository_root() / "examples" / example.directory);
        REQUIRE(trajectory.size() == 1'500'000);
        REQUIRE(trajectory.front().velocity == example.initial_velocity);
    }
}

TEST_CASE("all supplied Bosch examples execute through the normal engine and real FMU",
          "[functional][bosch][examples][integration]") {
    constexpr std::array<std::string_view, 3> directories{
        "example_v_10",
        "example_v_12_5",
        "example_v_15",
    };

    const cpssim::DefaultBoschRunService service;
    for (const auto directory : directories) {
        CAPTURE(directory);
        const auto result =
            service.run({.example_directory = repository_root() / "examples" / directory,
                         .shared_library = fmu_library(),
                         .scenario = cpssim::BoschReferenceScenario::SharedCloud,
                         .stop_tick = 2,
                         .reference_root = repository_root() / "experiments/bosch_v10_reference",
                         .instance_name = std::string{"bosch_example_"} + std::string{directory}});

        REQUIRE(result.functional_observation_count == 3);
        REQUIRE(result.final_observation.has_value());
        REQUIRE(result.final_observation->tick == 2);
        REQUIRE(result.canonical_event_count > 0);
    }
}

TEST_CASE("Bosch example loading reports a missing input directory",
          "[functional][bosch][examples][error]") {
    REQUIRE_THROWS_AS(
        cpssim::load_bosch_example_trajectory(repository_root() / "examples/not_present"),
        std::runtime_error);
}
