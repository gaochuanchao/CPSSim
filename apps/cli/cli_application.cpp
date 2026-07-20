/***
 * File: apps/cli/cli_application.cpp
 * Purpose: Implement persistent and direct CPSSim terminal command dispatch.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Command failures end direct execution but do not terminate a shell.
 ***/

#include "apps/cli/cli_application.hpp"

#include "apps/cli/command_parser.hpp"
#include "cpssim/core/version.hpp"

#include <exception>
#include <iostream>
#include <utility>

namespace cpssim {

/*** Stores only non-owning streams/service and an owned immutable path set. ***/
CliApplication::CliApplication(CliStreams streams, const BoschRunService& bosch_run_service,
                               CliPaths paths)
    : input_{streams.input}, output_{streams.output}, error_{streams.error},
      bosch_run_service_{bosch_run_service}, paths_{std::move(paths)} {}

/*** Chooses persistent shell or one direct registry dispatch. ***/
int CliApplication::run(const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        return run_shell();
    }
    return execute(arguments, false).exit_status;
}

/*** Resolves and invokes one command with a complete execution context. ***/
CliCommandResult CliApplication::execute(const std::vector<std::string>& tokens, bool interactive) {
    if (tokens.empty()) {
        return {};
    }
    const auto* command = registry_.find(tokens.front());
    if (command == nullptr) {
        error_ << "Unknown command: " << tokens.front() << "\nType \"help\" to list commands.\n";
        return {.exit_status = 2, .exit_shell = false};
    }

    const std::vector<std::string> arguments{tokens.begin() + 1, tokens.end()};
    CliCommandContext context{.input = input_,
                              .output = output_,
                              .error = error_,
                              .bosch_run_service = bosch_run_service_,
                              .registry = registry_,
                              .paths = paths_,
                              .interactive = interactive};
    return command->execute(arguments, context);
}

/*** Reads until an exit command or EOF while keeping command failures local. ***/
int CliApplication::run_shell() {
    output_ << "CPSSim " << version() << "\n"
            << "Interactive terminal interface\n"
            << "Type \"help\" to list commands.\n\n";

    std::string line;
    while (true) {
        output_ << "cpssim> " << std::flush;
        if (!std::getline(input_, line)) {
            output_ << '\n';
            return 0;
        }

        try {
            const auto result = execute(parse_cli_command_line(line), true);
            if (result.exit_shell) {
                return result.exit_status;
            }
        } catch (const std::exception& error) {
            error_ << "Command error: " << error.what() << '\n';
        }
    }
}

} // namespace cpssim
