/***
 * File: apps/cli/cli_application.hpp
 * Purpose: Declare direct and interactive execution of the CPSSim terminal
 *          command registry.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Streams and Bosch execution are injected for deterministic tests.
 ***/

#pragma once

#include "apps/cli/command_registry.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace cpssim {

/*** Names injected terminal streams so output and error cannot be swapped. ***/
struct CliStreams {
    std::istream& input;
    std::ostream& output;
    std::ostream& error;
};

/*** Coordinates the persistent shell and direct non-interactive commands. ***/
class CliApplication {
  public:
    CliApplication(CliStreams streams, const BoschRunService& bosch_run_service, CliPaths paths);

    /***
     * Enters the shell when arguments are empty; otherwise executes one direct
     * registry command and returns its status.
     ***/
    int run(const std::vector<std::string>& arguments);

  private:
    CliCommandResult execute(const std::vector<std::string>& tokens, bool interactive);
    int run_shell();

    std::istream& input_;
    std::ostream& output_;
    std::ostream& error_;
    const BoschRunService& bosch_run_service_;
    CliPaths paths_;
    CommandRegistry registry_;
};

} // namespace cpssim
