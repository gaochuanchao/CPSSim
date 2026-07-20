/***
 * File: apps/cli/commands/version_command.cpp
 * Purpose: Report the compiled CPSSim project version.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Version retrieval remains owned by cpssim_core.
 ***/

#include "apps/cli/commands/command_factories.hpp"

#include "cpssim/core/version.hpp"

#include <ostream>

namespace cpssim {
namespace {

class VersionCommand final : public CliCommand {
  public:
    std::string_view name() const override { return "version"; }
    std::string_view description() const override { return "Print the CPSSim version"; }
    std::string_view usage() const override { return "version"; }

    /*** Rejects arguments and prints the same version used by the shell banner. ***/
    CliCommandResult execute(const std::vector<std::string>& arguments,
                             CliCommandContext& context) const override {
        if (!arguments.empty()) {
            context.error << "Usage: " << usage() << '\n';
            return {.exit_status = 2};
        }
        context.output << "CPSSim " << version() << '\n';
        return {};
    }
};

} // namespace

/*** Creates the private version-command implementation. ***/
std::unique_ptr<CliCommand> make_version_command() { return std::make_unique<VersionCommand>(); }

} // namespace cpssim
