/***
 * File: apps/cli/commands/help_command.cpp
 * Purpose: Implement generated help for all registered CPSSim CLI commands.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Help metadata comes from commands rather than a duplicated table.
 ***/

#include "apps/cli/commands/command_factories.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>

namespace cpssim {
namespace {

class HelpCommand final : public CliCommand {
  public:
    std::string_view name() const override { return "help"; }
    std::string_view description() const override { return "List commands or show command usage"; }
    std::string_view usage() const override { return "help [command]"; }

    /*** Lists registry metadata or prints one exact command's usage. ***/
    CliCommandResult execute(const std::vector<std::string>& arguments,
                             CliCommandContext& context) const override {
        if (arguments.size() > 1) {
            context.error << "Usage: " << usage() << '\n';
            return {.exit_status = 2};
        }
        if (arguments.size() == 1) {
            const auto* command = context.registry.find(arguments.front());
            if (command == nullptr) {
                context.error << "Unknown command: " << arguments.front() << '\n';
                return {.exit_status = 2};
            }
            context.output << "Usage: " << command->usage() << '\n'
                           << command->description() << '\n';
            return {};
        }

        context.output << "Available commands:\n";
        std::size_t usage_width = 0;
        for (const auto& command : context.registry.commands()) {
            usage_width = std::max(usage_width, command->usage().size());
        }
        for (const auto& command : context.registry.commands()) {
            context.output << "  " << std::left << std::setw(static_cast<int>(usage_width + 2))
                           << command->usage() << command->description() << '\n';
        }
        return {};
    }
};

} // namespace

/*** Creates the private help-command implementation. ***/
std::unique_ptr<CliCommand> make_help_command() { return std::make_unique<HelpCommand>(); }

} // namespace cpssim
