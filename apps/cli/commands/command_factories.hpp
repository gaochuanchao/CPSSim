/***
 * File: apps/cli/commands/command_factories.hpp
 * Purpose: Declare factories used only by the central CLI command registry.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Command implementation types remain private to their source files.
 ***/

#pragma once

#include "apps/cli/command_registry.hpp"

#include <memory>
#include <string>

namespace cpssim {

std::unique_ptr<CliCommand> make_help_command();
std::unique_ptr<CliCommand> make_list_command();
std::unique_ptr<CliCommand> make_bosch_run_command();
std::unique_ptr<CliCommand> make_version_command();
std::unique_ptr<CliCommand> make_exit_command(std::string name);

} // namespace cpssim
