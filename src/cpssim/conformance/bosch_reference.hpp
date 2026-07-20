/***
 * File: src/cpssim/conformance/bosch_reference.hpp
 * Purpose: Declare Bosch timing-reference comparison and reusable online
 *          functional scenario execution.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This experiment-specific component links to the Bosch adapter and is
 *        not part of the generic simulator library.
 ***/

#pragma once

#include "cpssim/functional/functional_model.hpp"
#include "cpssim/model/experiment_config.hpp"
#include "cpssim/policy/resource_allocator.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

/*** Selects one of the two captured Bosch MATLAB timing scenarios. ***/
enum class BoschReferenceScenario {
    Dedicated,
    SharedCloud,
};

/*** Summarizes exact normalized comparison counts and the first divergence. ***/
struct ConformanceReport {
    BoschReferenceScenario scenario;
    bool matches;
    std::size_t expected_scheduler_events;
    std::size_t actual_scheduler_events;
    std::size_t expected_network_events;
    std::size_t actual_network_events;
    std::size_t expected_trigger_events;
    std::size_t actual_trigger_events;
    std::string first_divergence;
};

/*** Returns both traces produced by one reference scenario with a live model. ***/
struct BoschOnlineRun {
    std::vector<Event> canonical_trace;
    std::vector<FunctionalObservation> functional_trace;
};

/*** Reusable validated system and assignments for one pinned Bosch scenario. ***/
struct BoschReferenceInputs {
    ExperimentConfig config;
    std::vector<TaskAssignment> assignments;
};

// Returns the stable directory/command spelling for a reference scenario.
std::string_view bosch_reference_scenario_name(BoschReferenceScenario scenario);

/***
 * Converts `dedicated` or `shared_cloud` to its scenario value.
 * Throws std::invalid_argument for every other spelling.
 ***/
BoschReferenceScenario parse_bosch_reference_scenario(std::string_view name);

BoschReferenceInputs load_bosch_reference_inputs(
    const std::filesystem::path& reference_root, BoschReferenceScenario scenario);

/***
 * Loads one pinned scenario below reference_root, runs CPSSim through the
 * captured horizon, and compares scheduler, network, and trigger observations.
 * Throws when reference files are missing or malformed.
 ***/
ConformanceReport compare_bosch_reference(const std::filesystem::path& reference_root,
                                          BoschReferenceScenario scenario);

/***
 * Runs one pinned scenario with a non-owned functional model attached to its
 * normal SimulationEngine. Returns copied canonical and functional traces.
 ***/
BoschOnlineRun run_bosch_reference_online(const std::filesystem::path& reference_root,
                                          BoschReferenceScenario scenario,
                                          FunctionalModel& functional_model,
                                          Tick stop_tick = 150000);

// Formats counts, PASS/FAIL status, and any first-divergence details.
std::string format_conformance_report(const ConformanceReport& report);

} // namespace cpssim
