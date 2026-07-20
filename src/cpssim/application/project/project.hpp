/***
 * File: src/cpssim/application/project/project.hpp
 * Purpose: Declare GUI-application project metadata, context, and persistence.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include "cpssim/gui/simulation_session.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

inline constexpr std::uint32_t current_project_schema_version = 1;
inline constexpr std::uint32_t current_project_workspace_schema_version =
    current_gui_workspace_schema_version;

/*** Persistent metadata and project-owned relative file references. ***/
struct ProjectMetadata {
    std::uint32_t schema_version{current_project_schema_version};
    std::string name;
    std::filesystem::path system_file{"system.json"};
    std::filesystem::path workspace_file{"workspace.json"};
    std::filesystem::path default_run_plan{"run-plans/default.json"};
    std::optional<std::filesystem::path> scenario_file;
    std::string scenario_kind{"generic"};

    bool operator==(const ProjectMetadata&) const = default;
};

/*** Runtime-only ingredients used to reconstruct a project session. ***/
struct ProjectRuntimeInputs {
    GuiFunctionalModelFactory functional_model_factory;
    std::vector<GuiSignalDescriptor> signal_registry;
};

using ProjectRuntimeResolver =
    std::function<ProjectRuntimeInputs(const std::filesystem::path&, const ProjectMetadata&)>;
using ProjectContentWriter = std::function<void(const std::filesystem::path&)>;

using ProjectWorkspace = GuiWorkspaceState;

/*** Validated inputs for creation below a caller-selected parent directory. ***/
struct ProjectCreationRequest {
    std::filesystem::path parent_directory;
    std::string name;
    ExperimentConfig system;
    RunPlan default_run_plan;
    std::optional<std::filesystem::path> scenario_file;
    std::string scenario_kind{"generic"};
    ProjectWorkspace workspace{};
};

/*** Owns one loaded project and its sole active GUI simulation session. ***/
class ProjectContext {
  public:
    ProjectContext(std::filesystem::path root, ProjectMetadata metadata, RunPlan default_run_plan,
                   ProjectWorkspace workspace, std::unique_ptr<GuiSimulationSession> session,
                   ProjectRuntimeInputs runtime_inputs = {},
                   std::optional<std::string> workspace_diagnostic = std::nullopt);

    const std::filesystem::path& root() const { return root_; }
    const ProjectMetadata& metadata() const { return metadata_; }
    const RunPlan& default_run_plan() const { return default_run_plan_; }
    const ProjectWorkspace& workspace() const { return workspace_; }
    void set_workspace(ProjectWorkspace workspace);
    const std::optional<std::string>& workspace_diagnostic() const { return workspace_diagnostic_; }
    const ProjectRuntimeInputs& runtime_inputs() const { return runtime_inputs_; }
    GuiSimulationSession& session() { return *session_; }
    const GuiSimulationSession& session() const { return *session_; }

  private:
    std::filesystem::path root_;
    ProjectMetadata metadata_;
    RunPlan default_run_plan_;
    ProjectWorkspace workspace_;
    std::unique_ptr<GuiSimulationSession> session_;
    ProjectRuntimeInputs runtime_inputs_;
    std::optional<std::string> workspace_diagnostic_;
};

std::string serialize_project_metadata_json(const ProjectMetadata& metadata);
ProjectMetadata parse_project_metadata_json(std::string_view json_text);

std::string serialize_project_workspace_json(const ProjectWorkspace& workspace);
ProjectWorkspace parse_project_workspace_json(std::string_view json_text);

// Constructs and applies the validated default plan before returning ownership.
std::unique_ptr<ProjectContext>
make_project_context(const std::filesystem::path& root, ProjectMetadata metadata,
                     ExperimentConfig system, RunPlan default_run_plan,
                     ProjectWorkspace workspace = {}, ProjectRuntimeInputs runtime_inputs = {},
                     std::optional<std::string> workspace_diagnostic = std::nullopt);

// Creates <parent>/<name>, writes project.json last, and returns an active context.
std::unique_ptr<ProjectContext> create_project(const ProjectCreationRequest& request,
                                               ProjectRuntimeInputs runtime_inputs = {},
                                               const ProjectContentWriter& content_writer = {});

// Loads, validates, and constructs a complete replacement without changing GUI state.
std::unique_ptr<ProjectContext> load_project(const std::filesystem::path& project_file,
                                             const ProjectRuntimeResolver& runtime_resolver = {});

// Copies, validates, and constructs a complete replacement before returning it.
std::unique_ptr<ProjectContext>
save_project_as(const ProjectContext& project, const std::filesystem::path& parent_directory,
                std::string new_name, const ProjectRuntimeResolver& runtime_resolver = {});

// Saves specifications, the default plan, workspace, and metadata only.
void save_project(const ProjectContext& project);

} // namespace cpssim
