/***
 * File: tests/bosch/bosch_fmi2_functional_model_test.cpp
 * Purpose: Verify the Bosch v10 functional adapter's initialization,
 *          trajectory stepping, trigger translation, and boundary failures.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: CTest supplies the build-tree Bosch Linux shared-library path.
 ***/

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/functional/functional_runtime.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

/*** Returns the CMake-built Bosch Linux shared library. ***/
std::filesystem::path bosch_library_path() {
    const char* path = std::getenv("CPSSIM_T16_BOSCH_FMU_LIBRARY");
    if (path == nullptr || std::string{path}.empty()) {
        throw std::runtime_error{"CPSSIM_T16_BOSCH_FMU_LIBRARY is not set"};
    }
    return path;
}

/*** Creates the prepared identity declared by the Bosch model description. ***/
cpssim::Fmi2ModelInfo bosch_model_info(std::string instance_name) {
    return {.shared_library = bosch_library_path(),
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = std::move(instance_name)};
}

/*** Returns constant trajectory data for ticks zero through two. ***/
std::vector<cpssim::BoschTrajectorySample> short_trajectory() {
    return {{.feedforward_0 = 0.0, .feedforward_1 = 0.0, .velocity = 10.0},
            {.feedforward_0 = 0.0, .feedforward_1 = 0.0, .velocity = 10.0},
            {.feedforward_0 = 0.0, .feedforward_1 = 0.0, .velocity = 10.0}};
}

/*** Finds one named Real signal in the generic observation. ***/
double real_signal(const cpssim::FunctionalObservation& observation, const std::string& name) {
    for (const auto& signal : observation.real_signals) {
        if (signal.name == name) {
            return signal.value;
        }
    }
    throw std::logic_error{"Bosch observation lacks the requested Real signal"};
}

/*** Creates a valid Sensor JobStart action at the supplied tick. ***/
cpssim::Event sensor_start(cpssim::Tick tick, std::uint64_t sequence) {
    return {tick,
            cpssim::EventPhase::Scheduling,
            cpssim::EventSequence{sequence},
            cpssim::EventType::JobStart,
            {.task_id = cpssim::TaskId{1},
             .job_id = cpssim::JobId{1},
             .resource_id = cpssim::ResourceId{1},
             .message_id = std::nullopt,
             .vehicle_id = std::nullopt}};
}

} // namespace

/*** Verifies the initialized 0.1 lateral error and two physical steps. ***/
TEST_CASE("Bosch FMI functional model advances through generic runtime",
          "[functional][bosch][fmi2]") {
    cpssim::BoschFmi2FunctionalModel model{bosch_model_info("t16_short"), short_trajectory()};
    cpssim::FunctionalRuntime runtime{model, std::chrono::microseconds{100}, 2};

    const auto initial = runtime.initialize();
    runtime.apply_actions(0, {sensor_start(0, 0)});
    runtime.advance_to(2);
    runtime.finalize();

    const bool trace_matches = initial.size() == 1 && initial[0].tick == 0 &&
                               real_signal(initial[0], "lateral_error") == 0.1 &&
                               runtime.trace().size() == 3 && runtime.trace()[1].tick == 1 &&
                               runtime.trace()[2].tick == 2;
    REQUIRE(trace_matches);
}

/*** Verifies v10 timing, trajectory, and event-mapping failures are explicit. ***/
TEST_CASE("Bosch FMI functional model rejects invalid coupling input",
          "[functional][bosch][fmi2][error]") {
    cpssim::BoschFmi2FunctionalModel wrong_period{bosch_model_info("t16_period"),
                                                  short_trajectory()};
    cpssim::FunctionalRuntime period_runtime{wrong_period, std::chrono::milliseconds{1}, 2};
    REQUIRE_THROWS_AS(period_runtime.initialize(), std::invalid_argument);

    cpssim::BoschFmi2FunctionalModel short_input{bosch_model_info("t16_short_input"),
                                                 {short_trajectory().front()}};
    cpssim::FunctionalRuntime short_runtime{short_input, std::chrono::microseconds{100}, 2};
    REQUIRE_THROWS_AS(short_runtime.initialize(), std::invalid_argument);

    cpssim::BoschFmi2FunctionalModel bad_action{bosch_model_info("t16_bad_action"),
                                                short_trajectory()};
    cpssim::FunctionalRuntime action_runtime{bad_action, std::chrono::microseconds{100}, 2};
    action_runtime.initialize();
    const cpssim::Event unsupported{0,
                                    cpssim::EventPhase::Scheduling,
                                    cpssim::EventSequence{0},
                                    cpssim::EventType::JobStart,
                                    {.task_id = cpssim::TaskId{99},
                                     .job_id = cpssim::JobId{1},
                                     .resource_id = cpssim::ResourceId{1},
                                     .message_id = std::nullopt,
                                     .vehicle_id = std::nullopt}};
    REQUIRE_THROWS_AS(action_runtime.apply_actions(0, {unsupported}), std::logic_error);
}
