/***
 * File: apps/cli/main.cpp
 * Purpose: Construct the CPSSim terminal application and select direct or
 *          interactive command execution.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 * Notes: Command behavior is registered outside this process entry point.
 ***/

#include "apps/cli/cli_application.hpp"
#include "cpssim/application/bosch_run_service.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

/*** Resolves runtime paths beside the executable and delegates all commands. ***/
int main(int argument_count, char* arguments[]) {
    const auto executable = std::filesystem::absolute(arguments[0]);
    cpssim::CliPaths paths{.repository_root = std::filesystem::current_path(),
                           .fmu_shared_library =
                               executable.parent_path() / "LateralMotionControl.so"};
    const cpssim::DefaultBoschRunService bosch_run_service;
    cpssim::CliApplication application{{.input = std::cin, .output = std::cout, .error = std::cerr},
                                       bosch_run_service,
                                       std::move(paths)};

    std::vector<std::string> command_arguments;
    command_arguments.reserve(static_cast<std::size_t>(argument_count - 1));
    for (int index = 1; index < argument_count; ++index) {
        command_arguments.emplace_back(arguments[index]);
    }
    return application.run(command_arguments);
}
