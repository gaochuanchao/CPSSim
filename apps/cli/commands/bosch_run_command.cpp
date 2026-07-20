/***
 * File: apps/cli/commands/bosch_run_command.cpp
 * Purpose: Implement direct Bosch arguments and the interactive Bosch wizard.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Both paths create BoschRunRequest and call the same injected service;
 *        no scheduling, networking, trigger, or FMI semantics live here.
 ***/

#include "apps/cli/commands/command_factories.hpp"

#include <array>
#include <exception>
#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cpssim {
namespace {

constexpr std::array<std::string_view, 3> supported_examples{
    "example_v_10",
    "example_v_12_5",
    "example_v_15",
};

/*** Returns whether a path selects one of the three supplied trajectory names. ***/
bool is_supported_example(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    for (const auto supported : supported_examples) {
        if (name == supported) {
            return true;
        }
    }
    return false;
}

/*** Reads one wizard value and reports EOF as an explicit aborted command. ***/
std::string read_wizard_line(CliCommandContext& context, std::string_view prompt) {
    context.output << prompt << std::flush;
    std::string line;
    if (!std::getline(context.input, line)) {
        throw std::runtime_error{"input ended during the Bosch wizard"};
    }
    return line;
}

/*** Converts malformed numbered input to no choice for the retry loop. ***/
std::optional<Tick> try_parse_choice(std::string_view text) {
    try {
        return parse_bosch_stop_tick(text);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

/*** Repeats a numbered prompt until a value in the inclusive range is chosen. ***/
std::size_t prompt_choice(CliCommandContext& context, std::string_view prompt,
                          std::size_t maximum) {
    while (true) {
        const auto text = read_wizard_line(context, prompt);
        const auto value = try_parse_choice(text).value_or(0);
        if (value >= 1 && value <= static_cast<Tick>(maximum)) {
            return static_cast<std::size_t>(value);
        }
        context.error << "Choose a number from 1 to " << maximum << ".\n";
    }
}

/*** Converts direct options to the request consumed by the shared run service. ***/
BoschRunRequest parse_direct_request(const std::vector<std::string>& arguments,
                                     const CliPaths& paths) {
    BoschRunRequest request;
    request.shared_library = paths.fmu_shared_library;
    request.reference_root = paths.repository_root / "experiments/bosch_v10_reference";
    request.instance_name = "cpssim_cli_direct";
    bool has_example = false;
    bool has_scenario = false;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& option = arguments[index];
        if (option != "--example" && option != "--scenario" && option != "--stop-tick") {
            throw std::invalid_argument{"unknown Bosch option '" + option + "'"};
        }
        if (index + 1 >= arguments.size()) {
            throw std::invalid_argument{"missing value for Bosch option '" + option + "'"};
        }
        const auto& value = arguments[++index];
        if (option == "--example") {
            request.example_directory = std::filesystem::path{value};
            if (!is_supported_example(request.example_directory)) {
                throw std::invalid_argument{
                    "trajectory must be example_v_10, example_v_12_5, or example_v_15"};
            }
            if (request.example_directory.is_relative() &&
                !request.example_directory.has_parent_path()) {
                request.example_directory =
                    paths.repository_root / "examples" / request.example_directory;
            } else if (request.example_directory.is_relative()) {
                request.example_directory = paths.repository_root / request.example_directory;
            }
            const auto supplied_directory =
                paths.repository_root / "examples" / request.example_directory.filename();
            if (request.example_directory.lexically_normal() !=
                supplied_directory.lexically_normal()) {
                throw std::invalid_argument{
                    "trajectory path must select a supplied examples/ directory"};
            }
            has_example = true;
        } else if (option == "--scenario") {
            request.scenario = parse_bosch_reference_scenario(value);
            has_scenario = true;
        } else {
            request.stop_tick = parse_bosch_stop_tick(value);
        }
    }

    if (!has_example || !has_scenario) {
        throw std::invalid_argument{"--example and --scenario are required for a direct Bosch run"};
    }
    return request;
}

/*** Prompts only for user choices and returns the same request type as direct mode. ***/
BoschRunRequest prompt_for_request(CliCommandContext& context) {
    context.output << "Bosch Challenge experiment\n\n"
                   << "Trajectory:\n"
                   << "  1. example_v_10\n"
                   << "  2. example_v_12_5\n"
                   << "  3. example_v_15\n";
    const auto example = prompt_choice(context, "Selection: ", supported_examples.size());

    context.output << "\nScenario:\n"
                   << "  1. dedicated\n"
                   << "  2. shared_cloud\n";
    const auto scenario = prompt_choice(context, "Selection: ", 2);

    context.output << "\nHorizon:\n"
                   << "  1. Complete supplied trajectory\n"
                   << "  2. Enter a nonnegative stop tick\n";
    const auto horizon = prompt_choice(context, "Selection: ", 2);

    BoschRunRequest request;
    request.example_directory =
        context.paths.repository_root / "examples" / std::string{supported_examples[example - 1]};
    request.shared_library = context.paths.fmu_shared_library;
    request.reference_root = context.paths.repository_root / "experiments/bosch_v10_reference";
    request.scenario =
        scenario == 1 ? BoschReferenceScenario::Dedicated : BoschReferenceScenario::SharedCloud;
    request.instance_name = "cpssim_cli_interactive";
    if (horizon == 2) {
        request.stop_tick = parse_bosch_stop_tick(read_wizard_line(context, "Stop tick: "));
    }
    return request;
}

/*** Shows the complete request before the shared service begins execution. ***/
void print_request(const BoschRunRequest& request, std::ostream& output) {
    output << "\nRun summary\n"
           << "  Trajectory: " << request.example_directory.filename().string() << '\n'
           << "  Scenario: " << bosch_reference_scenario_name(request.scenario) << '\n'
           << "  Stop tick: ";
    if (request.stop_tick.has_value()) {
        output << *request.stop_tick;
    } else {
        output << "complete supplied trajectory";
    }
    output << "\n\nRunning Bosch experiment...\n";
}

/*** Renders the service result without interpreting simulation events. ***/
void print_result(const BoschRunSummary& summary, std::ostream& output) {
    output << "Bosch example: " << summary.example_directory.string() << '\n'
           << "Scenario: " << bosch_reference_scenario_name(summary.scenario) << '\n'
           << "Stop tick: " << summary.stop_tick << '\n'
           << "Canonical events: " << summary.canonical_event_count << '\n'
           << "Functional observations: " << summary.functional_observation_count << '\n';
    if (!summary.final_observation.has_value()) {
        return;
    }

    const auto& final = *summary.final_observation;
    output << "Final observation tick: " << final.tick << '\n';
    for (const auto& signal : final.real_signals) {
        output << signal.name << ": " << signal.value << '\n';
    }
    for (const auto& signal : final.integer_signals) {
        output << signal.name << ": " << signal.value << '\n';
    }
    for (const auto& signal : final.boolean_signals) {
        output << signal.name << ": " << (signal.value ? "true" : "false") << '\n';
    }
}

class BoschRunCommand final : public CliCommand {
  public:
    std::string_view name() const override { return "run"; }
    std::string_view description() const override {
        return "Run an experiment; 'run bosch' opens the Bosch wizard";
    }
    std::string_view usage() const override {
        return "run bosch [--example DIR --scenario NAME [--stop-tick TICK]]";
    }

    /*** Selects prompting or direct parsing, then invokes the injected service once. ***/
    CliCommandResult execute(const std::vector<std::string>& arguments,
                             CliCommandContext& context) const override {
        if (arguments.size() == 2 && arguments[0] == "bosch" && arguments[1] == "--help") {
            context.output << "Usage: " << usage() << '\n';
            return {};
        }
        if (arguments.empty() || arguments.front() != "bosch") {
            context.error << "Usage: " << usage() << '\n';
            return {.exit_status = 2};
        }

        try {
            BoschRunRequest request;
            if (context.interactive && arguments.size() == 1) {
                request = prompt_for_request(context);
            } else {
                const std::vector<std::string> options{arguments.begin() + 1, arguments.end()};
                request = parse_direct_request(options, context.paths);
            }
            print_request(request, context.output);
            print_result(context.bosch_run_service.run(request), context.output);
            return {};
        } catch (const std::exception& error) {
            context.error << "Bosch run error: " << error.what() << '\n';
            return {.exit_status = 2};
        }
    }
};

} // namespace

/*** Creates the private Bosch run-command implementation. ***/
std::unique_ptr<CliCommand> make_bosch_run_command() { return std::make_unique<BoschRunCommand>(); }

} // namespace cpssim
