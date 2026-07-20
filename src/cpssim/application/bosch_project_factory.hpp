/***
 * File: src/cpssim/application/bosch_project_factory.hpp
 * Purpose: Declare non-running Bosch project/session construction for the GUI.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include "cpssim/application/project/project.hpp"
#include "cpssim/conformance/bosch_reference.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace cpssim {

struct BoschProjectRequest {
    std::filesystem::path parent_directory;
    std::string name;
    std::filesystem::path trajectory_directory;
    BoschReferenceScenario scenario{BoschReferenceScenario::Dedicated};
    std::optional<Tick> stop_tick;
    std::filesystem::path reference_root;
    std::filesystem::path shared_library;
};

void validate_bosch_project_request(const BoschProjectRequest& request);

std::filesystem::path resolve_bundled_bosch_fmu(
    const std::filesystem::path& executable_path);

std::unique_ptr<ProjectContext> create_bosch_project(const BoschProjectRequest& request);

ProjectRuntimeInputs resolve_bosch_project_runtime(
    const std::filesystem::path& project_root, const ProjectMetadata& metadata,
    const std::filesystem::path& reference_root,
    const std::filesystem::path& shared_library);

} // namespace cpssim
