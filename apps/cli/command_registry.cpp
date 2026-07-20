/***
 * File: apps/cli/command_registry.cpp
 * Purpose: Register and resolve the complete CPSSim CLI command set.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: This constructor is the only command-registration point.
 ***/

#include "apps/cli/command_registry.hpp"

#include "apps/cli/commands/command_factories.hpp"

namespace cpssim {

/*** Registers commands in the stable order used by help output. ***/
CommandRegistry::CommandRegistry() {
    commands_.push_back(make_help_command());
    commands_.push_back(make_list_command());
    commands_.push_back(make_bosch_run_command());
    commands_.push_back(make_version_command());
    commands_.push_back(make_exit_command("quit"));
    commands_.push_back(make_exit_command("exit"));
}

/*** Returns the exact registered name or nullptr when no command matches. ***/
const CliCommand* CommandRegistry::find(std::string_view name) const {
    for (const auto& command : commands_) {
        if (command->name() == name) {
            return command.get();
        }
    }
    return nullptr;
}

/*** Exposes read-only command metadata for generated help output. ***/
const std::vector<std::unique_ptr<CliCommand>>& CommandRegistry::commands() const noexcept {
    return commands_;
}

} // namespace cpssim
