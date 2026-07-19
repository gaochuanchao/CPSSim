/***
 * File: tests/conformance/bosch_reference_test.cpp
 * Purpose: Verify scenario parsing, exact captured scheduler/network/trigger
 *          conformance, and useful first-divergence reporting.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/conformance/bosch_reference.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

// Resolves tracked test data from this source file without an editor macro.
std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
}

} // namespace

TEST_CASE("Bosch reference scenario names have a strict stable spelling") {
    const bool names_match = cpssim::parse_bosch_reference_scenario("dedicated") ==
                                 cpssim::BoschReferenceScenario::Dedicated &&
                             cpssim::parse_bosch_reference_scenario("shared_cloud") ==
                                 cpssim::BoschReferenceScenario::SharedCloud;
    REQUIRE(names_match);
    REQUIRE_THROWS_AS(cpssim::parse_bosch_reference_scenario("cloud"), std::invalid_argument);
}

TEST_CASE("dedicated Bosch timing matches the MATLAB reference") {
    const auto report =
        cpssim::compare_bosch_reference(repository_root() / "experiments/bosch_v10_reference",
                                        cpssim::BoschReferenceScenario::Dedicated);

    INFO(cpssim::format_conformance_report(report));
    const bool counts_match =
        report.expected_scheduler_events == 21762 && report.actual_scheduler_events == 21762 &&
        report.expected_network_events == 3750 && report.actual_network_events == 3750 &&
        report.expected_trigger_events == 22005 && report.actual_trigger_events == 22005;
    REQUIRE(report.matches);
    REQUIRE(counts_match);
    REQUIRE(report.first_divergence.empty());
}

TEST_CASE("shared-cloud Bosch timing matches the MATLAB reference") {
    const auto report =
        cpssim::compare_bosch_reference(repository_root() / "experiments/bosch_v10_reference",
                                        cpssim::BoschReferenceScenario::SharedCloud);

    INFO(cpssim::format_conformance_report(report));
    const bool counts_match =
        report.expected_scheduler_events == 21759 && report.actual_scheduler_events == 21759 &&
        report.expected_network_events == 3750 && report.actual_network_events == 3750 &&
        report.expected_trigger_events == 22002 && report.actual_trigger_events == 22002;
    REQUIRE(report.matches);
    REQUIRE(counts_match);
    REQUIRE(report.first_divergence.empty());
}

TEST_CASE("an altered scheduler row reports the first divergence") {
    const auto report =
        cpssim::compare_bosch_reference(repository_root() / "tests/fixtures/t12_first_divergence",
                                        cpssim::BoschReferenceScenario::Dedicated);

    const bool diagnostic_is_complete =
        report.first_divergence.find("stream: scheduler_events") != std::string::npos &&
        report.first_divergence.find("row: 1") != std::string::npos &&
        report.first_divergence.find("previous matching row: none") != std::string::npos &&
        report.first_divergence.find("expected:") != std::string::npos &&
        report.first_divergence.find("actual:") != std::string::npos;
    REQUIRE_FALSE(report.matches);
    REQUIRE(diagnostic_is_complete);
}
