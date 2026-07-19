/***
 * File: apps/conformance/main.cpp
 * Purpose: Provide a terminal entry point for captured Bosch scheduler,
 *          network, and trigger conformance comparisons.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Run from the repository root with no arguments, or pass a reference
 *        root followed by one scenario name.
 ***/

#include "cpssim/conformance/bosch_reference.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

/*** Runs one selected scenario, prints its report, and returns pass/fail. ***/
bool run_scenario(const std::filesystem::path& reference_root,
                  cpssim::BoschReferenceScenario scenario) {
    const auto report = cpssim::compare_bosch_reference(reference_root, scenario);
    std::cout << cpssim::format_conformance_report(report) << '\n';
    return report.matches;
}

} // namespace

/***
 * Runs both scenarios with no arguments, or one scenario when reference root
 * and scenario name are supplied. Returns 0 for conformance, 1 for a timing
 * divergence, and 2 for invalid input or malformed reference data.
 ***/
int main(int argument_count, char* arguments[]) {
    try {
        if (argument_count == 1) {
            const std::filesystem::path reference_root{"experiments/bosch_v10_reference"};
            const bool dedicated =
                run_scenario(reference_root, cpssim::BoschReferenceScenario::Dedicated);
            const bool shared =
                run_scenario(reference_root, cpssim::BoschReferenceScenario::SharedCloud);
            return dedicated && shared ? 0 : 1;
        }
        if (argument_count == 3) {
            const std::filesystem::path reference_root{arguments[1]};
            const auto scenario =
                cpssim::parse_bosch_reference_scenario(std::string_view{arguments[2]});
            return run_scenario(reference_root, scenario) ? 0 : 1;
        }

        std::cerr << "Usage: cpssim_bosch_conformance [REFERENCE_ROOT SCENARIO]\n"
                     "SCENARIO must be dedicated or shared_cloud.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Conformance error: " << error.what() << '\n';
        return 2;
    }
}
