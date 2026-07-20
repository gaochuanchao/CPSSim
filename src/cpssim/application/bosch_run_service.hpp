/***
 * File: src/cpssim/application/bosch_run_service.hpp
 * Purpose: Declare reusable application-level orchestration for one supplied
 *          Bosch trajectory run.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: This service sits above the Bosch and FMI adapters. It is not part of
 *        the generic simulator core and contains no terminal prompting.
 ***/

#pragma once

#include "cpssim/conformance/bosch_reference.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace cpssim {

/*** Contains all parser-independent inputs for one supplied Bosch run. ***/
struct BoschRunRequest {
    std::filesystem::path example_directory;
    std::filesystem::path shared_library;
    BoschReferenceScenario scenario{BoschReferenceScenario::SharedCloud};
    std::optional<Tick> stop_tick;
    std::filesystem::path reference_root{"experiments/bosch_v10_reference"};
    std::string instance_name{"cpssim_bosch_run"};
};

/*** Contains the stable summary data presented by terminal applications. ***/
struct BoschRunSummary {
    std::filesystem::path example_directory;
    BoschReferenceScenario scenario;
    Tick stop_tick;
    std::size_t canonical_event_count;
    std::size_t functional_observation_count;
    std::optional<FunctionalObservation> final_observation;
};

/***
 * Provides an injectable application boundary for Bosch execution. Terminal
 * parsing and prompting depend on this interface, not on FMI or engine calls.
 ***/
class BoschRunService {
  public:
    virtual ~BoschRunService() = default;

    /***
     * Executes a validated request through the existing Bosch scheduling,
     * networking, trigger, and FMI path. Failures are reported by exception.
     ***/
    virtual BoschRunSummary run(const BoschRunRequest& request) const = 0;
};

/*** Implements BoschRunService with the production CPSSim adapters. ***/
class DefaultBoschRunService final : public BoschRunService {
  public:
    BoschRunSummary run(const BoschRunRequest& request) const override;
};

/***
 * Parses one nonnegative integer Tick without signs, whitespace, or suffixes.
 * Throws std::invalid_argument when the complete text is not representable.
 ***/
Tick parse_bosch_stop_tick(std::string_view text);

} // namespace cpssim
