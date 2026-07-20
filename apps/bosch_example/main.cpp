/***
 * File: apps/bosch_example/main.cpp
 * Purpose: Preserve the legacy Bosch integration executable over the shared
 *          application run service.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 * Notes: The public user path is now `cpssim_cli run bosch`; this executable
 *        remains useful as an internal compatibility runner.
 ***/

#include "cpssim/application/bosch_run_service.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

/*** Prints compatibility syntax for callers of the internal executable. ***/
void print_usage() {
    std::cerr << "Usage: cpssim_bosch_example EXAMPLE_DIR FMU_LIBRARY "
                 "[SCENARIO [STOP_TICK [REFERENCE_ROOT]]]\n"
                 "SCENARIO defaults to shared_cloud and must be dedicated or shared_cloud.\n"
                 "STOP_TICK defaults to the final supplied sample.\n";
}

/*** Prints the stable Bosch summary and typed final observation. ***/
void print_summary(const cpssim::BoschRunSummary& summary) {
    std::cout << "Bosch example: " << summary.example_directory.string() << '\n'
              << "Scenario: " << cpssim::bosch_reference_scenario_name(summary.scenario) << '\n'
              << "Stop tick: " << summary.stop_tick << '\n'
              << "Canonical events: " << summary.canonical_event_count << '\n'
              << "Functional observations: " << summary.functional_observation_count << '\n';
    if (!summary.final_observation.has_value()) {
        return;
    }

    const auto& final = *summary.final_observation;
    std::cout << "Final observation tick: " << final.tick << '\n';
    for (const auto& signal : final.real_signals) {
        std::cout << signal.name << ": " << signal.value << '\n';
    }
    for (const auto& signal : final.integer_signals) {
        std::cout << signal.name << ": " << signal.value << '\n';
    }
    for (const auto& signal : final.boolean_signals) {
        std::cout << signal.name << ": " << (signal.value ? "true" : "false") << '\n';
    }
}

} // namespace

/*** Converts legacy arguments to one shared service request; failures return 2. ***/
int main(int argument_count, char* arguments[]) {
    try {
        if (argument_count < 3 || argument_count > 6) {
            print_usage();
            return 2;
        }

        cpssim::BoschRunRequest request;
        request.example_directory = std::filesystem::path{arguments[1]};
        request.shared_library = std::filesystem::path{arguments[2]};
        request.instance_name = "cpssim_bosch_example";
        if (argument_count >= 4) {
            request.scenario =
                cpssim::parse_bosch_reference_scenario(std::string_view{arguments[3]});
        }
        if (argument_count >= 5) {
            request.stop_tick = cpssim::parse_bosch_stop_tick(arguments[4]);
        }
        if (argument_count >= 6) {
            request.reference_root = std::filesystem::path{arguments[5]};
        }

        const cpssim::DefaultBoschRunService service;
        print_summary(service.run(request));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Bosch example error: " << error.what() << '\n';
        return 2;
    }
}
