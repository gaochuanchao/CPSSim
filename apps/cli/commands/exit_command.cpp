/***
 * File: apps/cli/commands/exit_command.cpp
 * Purpose: Implement the equivalent quit and exit shell commands.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Each spelling is registered explicitly for discoverable help output.
 ***/

#include "apps/cli/commands/command_factories.hpp"

#include <ostream>
#include <utility>

namespace cpssim {
namespace {

class ExitCommand final : public CliCommand {
  public:
    explicit ExitCommand(std::string name) : name_{std::move(name)} {}

    std::string_view name() const override { return name_; }
    std::string_view description() const override { return "Leave the terminal interface"; }
    std::string_view usage() const override { return name_; }

    /*** Requests a clean shell exit; direct use also succeeds immediately. ***/
    CliCommandResult execute(const std::vector<std::string>& arguments,
                             CliCommandContext& context) const override {
        if (!arguments.empty()) {
            context.error << "Usage: " << usage() << '\n';
            return {.exit_status = 2};
        }
        return {.exit_status = 0, .exit_shell = true};
    }

  private:
    std::string name_;
};

} // namespace

/*** Creates one private exit-command implementation with the requested spelling. ***/
std::unique_ptr<CliCommand> make_exit_command(std::string name) {
    return std::make_unique<ExitCommand>(std::move(name));
}

} // namespace cpssim
