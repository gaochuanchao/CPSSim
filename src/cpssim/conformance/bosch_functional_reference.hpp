/***
 * File: src/cpssim/conformance/bosch_functional_reference.hpp
 * Purpose: Declare Bosch online-functional conformance and offline-replay
 *          comparison against the captured MATLAB/Simulink reference.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This experiment-specific interface is outside cpssim_core.
 ***/

#pragma once

#include "cpssim/conformance/bosch_reference.hpp"
#include "cpssim/fmi/fmi2_importer.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace cpssim {

/*** Defines the documented Real-signal comparison rule. ***/
struct FunctionalTolerance {
    double absolute;
    double relative;
};

/*** Summarizes one complete 15-second functional conformance comparison. ***/
struct BoschFunctionalConformanceReport {
    BoschReferenceScenario scenario;
    bool matches;
    bool online_replay_matches;
    std::size_t expected_rows;
    std::size_t actual_rows;
    double max_absolute_error;
    double max_relative_error;
    FunctionalTolerance tolerance;
    std::string first_divergence;
};

/***
 * Loads the pinned trajectory and functional CSV, runs the normal online
 * engine with a Bosch FMI model, and replays its canonical trace in a second
 * FMI instance. Integer and Boolean values are compared exactly; Real values
 * use the returned fixed absolute-plus-relative tolerance.
 ***/
BoschFunctionalConformanceReport
compare_bosch_functional_reference(const std::filesystem::path& reference_root,
                                   BoschReferenceScenario scenario,
                                   const Fmi2ModelInfo& model_info);

// Formats row counts, measured errors, replay status, and first divergence.
std::string format_bosch_functional_report(const BoschFunctionalConformanceReport& report);

} // namespace cpssim
