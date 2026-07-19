/***
 * File: apps/bosch_example/main.cpp
 * Purpose: Run one supplied Bosch example trajectory through CPSSim's normal
 *          scheduling, networking, trigger, and FMI functional path.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The explicit dedicated/shared-cloud run plan is loaded from the
 *        already validated Bosch v10 reference fixture.
 ***/

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/bosch/example_data.hpp"
#include "cpssim/conformance/bosch_reference.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

/*** Parses one nonnegative stop tick without accepting signs or suffixes. ***/
cpssim::Tick parse_stop_tick(std::string_view text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value > static_cast<std::uint64_t>(std::numeric_limits<cpssim::Tick>::max())) {
        throw std::invalid_argument{"STOP_TICK must be a nonnegative integer Tick"};
    }
    return static_cast<cpssim::Tick>(value);
}

/*** Builds the fixed identity exported by the supplied Bosch v10 FMU. ***/
cpssim::Fmi2ModelInfo model_info(const std::filesystem::path& shared_library,
                                 std::string instance_name) {
    return {.shared_library = shared_library,
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = std::move(instance_name)};
}

void print_usage() {
    std::cerr << "Usage: cpssim_bosch_example EXAMPLE_DIR FMU_LIBRARY "
                 "[SCENARIO [STOP_TICK [REFERENCE_ROOT]]]\n"
                 "SCENARIO defaults to shared_cloud and must be dedicated or shared_cloud.\n"
                 "STOP_TICK defaults to the final supplied sample.\n";
}

} // namespace

/*** Loads, runs, and reports one Bosch example; errors return status 2. ***/
int main(int argument_count, char* arguments[]) {
    try {
        if (argument_count < 3 || argument_count > 6) {
            print_usage();
            return 2;
        }

        const std::filesystem::path example_directory{arguments[1]};
        const std::filesystem::path shared_library{arguments[2]};
        const auto scenario =
            argument_count >= 4
                ? cpssim::parse_bosch_reference_scenario(std::string_view{arguments[3]})
                : cpssim::BoschReferenceScenario::SharedCloud;
        const std::filesystem::path reference_root =
            argument_count >= 6 ? arguments[5] : "experiments/bosch_v10_reference";

        auto trajectory = cpssim::load_bosch_example_trajectory(example_directory);
        const auto final_input_tick = static_cast<cpssim::Tick>(trajectory.size() - 1);
        const auto stop_tick =
            argument_count >= 5 ? parse_stop_tick(arguments[4]) : final_input_tick;
        if (stop_tick > final_input_tick) {
            throw std::invalid_argument{"STOP_TICK exceeds the final supplied example tick"};
        }

        cpssim::BoschFmi2FunctionalModel model{model_info(shared_library, "cpssim_bosch_example"),
                                               std::move(trajectory)};
        const auto run =
            cpssim::run_bosch_reference_online(reference_root, scenario, model, stop_tick);

        std::cout << "Bosch example: " << example_directory.string() << '\n'
                  << "Scenario: " << cpssim::bosch_reference_scenario_name(scenario) << '\n'
                  << "Stop tick: " << stop_tick << '\n'
                  << "Canonical events: " << run.canonical_trace.size() << '\n'
                  << "Functional observations: " << run.functional_trace.size() << '\n';
        if (!run.functional_trace.empty()) {
            const auto& final = run.functional_trace.back();
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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Bosch example error: " << error.what() << '\n';
        return 2;
    }
}
