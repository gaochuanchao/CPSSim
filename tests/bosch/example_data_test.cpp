/***
 * File: tests/bosch/example_data_test.cpp
 * Purpose: Verify strict loading and real-FMU execution of every supplied
 *          Bosch example trajectory.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: CTest supplies the build-tree Bosch Linux shared-library path.
 ***/

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/bosch/example_data.hpp"
#include "cpssim/conformance/bosch_reference.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
}

cpssim::Fmi2ModelInfo model_info(std::string instance_name) {
    const char* path = std::getenv("CPSSIM_T16_BOSCH_FMU_LIBRARY");
    if (path == nullptr || std::string{path}.empty()) {
        throw std::runtime_error{"CPSSIM_T16_BOSCH_FMU_LIBRARY is not set"};
    }
    return {.shared_library = path,
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = std::move(instance_name)};
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

    std::size_t instance = 0;
    for (const auto directory : directories) {
        CAPTURE(directory);
        auto trajectory =
            cpssim::load_bosch_example_trajectory(repository_root() / "examples" / directory);
        cpssim::BoschFmi2FunctionalModel model{
            model_info("bosch_example_" + std::to_string(instance++)), std::move(trajectory)};
        const auto run = cpssim::run_bosch_reference_online(
            repository_root() / "experiments/bosch_v10_reference",
            cpssim::BoschReferenceScenario::SharedCloud, model, 2);

        REQUIRE(run.functional_trace.size() == 3);
        REQUIRE(run.functional_trace.back().tick == 2);
        REQUIRE_FALSE(run.canonical_trace.empty());
    }
}

TEST_CASE("Bosch example loading reports a missing input directory",
          "[functional][bosch][examples][error]") {
    REQUIRE_THROWS_AS(
        cpssim::load_bosch_example_trajectory(repository_root() / "examples/not_present"),
        std::runtime_error);
}
