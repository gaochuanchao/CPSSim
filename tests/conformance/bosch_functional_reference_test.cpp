/***
 * File: tests/conformance/bosch_functional_reference_test.cpp
 * Purpose: Compare direct Linux Bosch FMI execution with both captured
 *          15-second functional traces and with canonical offline replay.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Expected CSVs remain immutable MATLAB/Simulink reference artifacts.
 ***/

#include "cpssim/conformance/bosch_functional_reference.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

/*** Resolves tracked reference data relative to this source file. ***/
std::filesystem::path reference_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() /
           "experiments/bosch_v10_reference";
}

/*** Returns the generated Linux model identity for one conformance run. ***/
cpssim::Fmi2ModelInfo model_info(const std::string& instance_name) {
    const char* path = std::getenv("CPSSIM_T16_BOSCH_FMU_LIBRARY");
    if (path == nullptr || std::string{path}.empty()) {
        throw std::runtime_error{"CPSSIM_T16_BOSCH_FMU_LIBRARY is not set"};
    }
    return {.shared_library = path,
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = instance_name};
}

} // namespace

/*** Verifies every dedicated-resource functional sample and replay row. ***/
TEST_CASE("dedicated Bosch functional output matches the captured reference",
          "[functional][conformance][bosch][dedicated]") {
    const auto report = cpssim::compare_bosch_functional_reference(
        reference_root(), cpssim::BoschReferenceScenario::Dedicated, model_info("t16_dedicated"));
    INFO(cpssim::format_bosch_functional_report(report));
    REQUIRE(report.matches);
    REQUIRE(report.online_replay_matches);
}

/*** Verifies every shared-cloud functional sample and replay row. ***/
TEST_CASE("shared-cloud Bosch functional output matches the captured reference",
          "[functional][conformance][bosch][shared-cloud]") {
    const auto report = cpssim::compare_bosch_functional_reference(
        reference_root(), cpssim::BoschReferenceScenario::SharedCloud,
        model_info("t16_shared_cloud"));
    INFO(cpssim::format_bosch_functional_report(report));
    REQUIRE(report.matches);
    REQUIRE(report.online_replay_matches);
}
