/***
 * File: apps/cli/command_registry.hpp
 * Purpose: Define the single registration and dispatch boundary for CPSSim
 *          terminal commands.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Adding a CLI command requires one registration here and no Makefile
 *        change.
 ***/

#pragma once

#include "cpssim/application/bosch_run_service.hpp"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

class CommandRegistry;

/*** Supplies repository and build-tree paths without embedding them in commands. ***/
struct CliPaths {
    std::filesystem::path repository_root;
    std::filesystem::path fmu_shared_library;
};

/*** Reports command status and whether an interactive shell should close. ***/
struct CliCommandResult {
    int exit_status{0};
    bool exit_shell{false};
};

/*** Supplies the streams, services, registry, and mode used by one command. ***/
struct CliCommandContext {
    std::istream& input;
    std::ostream& output;
    std::ostream& error;
    const BoschRunService& bosch_run_service;
    const CommandRegistry& registry;
    const CliPaths& paths;
    bool interactive;
};

/*** Defines metadata and execution for one registered terminal command. ***/
class CliCommand {
  public:
    virtual ~CliCommand() = default;

    virtual std::string_view name() const = 0;
    virtual std::string_view description() const = 0;
    virtual std::string_view usage() const = 0;
    virtual CliCommandResult execute(const std::vector<std::string>& arguments,
                                     CliCommandContext& context) const = 0;
};

/*** Owns every available CLI command and resolves exact command names. ***/
class CommandRegistry {
  public:
    CommandRegistry();

    const CliCommand* find(std::string_view name) const;
    const std::vector<std::unique_ptr<CliCommand>>& commands() const noexcept;

  private:
    std::vector<std::unique_ptr<CliCommand>> commands_;
};

} // namespace cpssim
