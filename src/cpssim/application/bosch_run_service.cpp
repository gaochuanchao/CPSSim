/***
 * File: src/cpssim/application/bosch_run_service.cpp
 * Purpose: Implement shared Bosch example loading and online execution.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: All canonical execution remains delegated to the existing reference
 *        runner and Bosch functional adapter.
 ***/

#include "cpssim/application/bosch_run_service.hpp"

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/bosch/example_data.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cpssim {
namespace {

/*** Builds the fixed model identity exported by the supplied Bosch v10 FMU. ***/
Fmi2ModelInfo model_info(const BoschRunRequest& request) {
    return {.shared_library = request.shared_library,
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = request.instance_name};
}

} // namespace

/*** Validates the complete decimal token before narrowing to signed Tick. ***/
Tick parse_bosch_stop_tick(std::string_view text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value > static_cast<std::uint64_t>(std::numeric_limits<Tick>::max())) {
        throw std::invalid_argument{"stop tick must be a nonnegative integer Tick"};
    }
    return static_cast<Tick>(value);
}

/*** Loads the trajectory, resolves its inclusive horizon, and delegates the run. ***/
BoschRunSummary DefaultBoschRunService::run(const BoschRunRequest& request) const {
    if (request.example_directory.empty()) {
        throw std::invalid_argument{"Bosch example directory must not be empty"};
    }
    if (request.shared_library.empty()) {
        throw std::invalid_argument{"Bosch FMU shared-library path must not be empty"};
    }
    if (request.reference_root.empty()) {
        throw std::invalid_argument{"Bosch reference root must not be empty"};
    }
    if (request.stop_tick.has_value() && *request.stop_tick < 0) {
        throw std::invalid_argument{"stop tick must be nonnegative"};
    }

    auto trajectory = load_bosch_example_trajectory(request.example_directory);
    const auto final_input_tick = static_cast<Tick>(trajectory.size() - 1);
    const auto stop_tick = request.stop_tick.value_or(final_input_tick);
    if (stop_tick > final_input_tick) {
        throw std::invalid_argument{"stop tick exceeds the final supplied example tick"};
    }

    BoschFmi2FunctionalModel model{model_info(request), std::move(trajectory)};
    auto run =
        run_bosch_reference_online(request.reference_root, request.scenario, model, stop_tick);

    BoschRunSummary summary{.example_directory = request.example_directory,
                            .scenario = request.scenario,
                            .stop_tick = stop_tick,
                            .canonical_event_count = run.canonical_trace.size(),
                            .functional_observation_count = run.functional_trace.size(),
                            .final_observation = std::nullopt};
    if (!run.functional_trace.empty()) {
        summary.final_observation = std::move(run.functional_trace.back());
    }
    return summary;
}

} // namespace cpssim
