/***
 * File: apps/cli/commands/list_command.cpp
 * Purpose: List experiment families currently exposed by the CPSSim CLI.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: This is discoverability metadata, not simulation configuration.
 ***/

#include "apps/cli/commands/command_factories.hpp"

#include <ostream>

namespace cpssim {
namespace {

class ListCommand final : public CliCommand {
  public:
    std::string_view name() const override { return "list"; }
    std::string_view description() const override { return "List available experiment families"; }
    std::string_view usage() const override { return "list"; }

    /*** Reports the currently supported Bosch family and its fixed choices. ***/
    CliCommandResult execute(const std::vector<std::string>& arguments,
                             CliCommandContext& context) const override {
        if (!arguments.empty()) {
            context.error << "Usage: " << usage() << '\n';
            return {.exit_status = 2};
        }
        context.output << "Available experiment families:\n"
                       << "  bosch  Supplied example_v_10, example_v_12_5, and example_v_15 "
                          "trajectories\n"
                       << "         Scenarios: dedicated, shared_cloud\n";
        return {};
    }
};

} // namespace

/*** Creates the private list-command implementation. ***/
std::unique_ptr<CliCommand> make_list_command() { return std::make_unique<ListCommand>(); }

} // namespace cpssim
