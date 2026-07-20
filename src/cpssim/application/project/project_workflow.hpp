/*** Testable project/run-plan operations driven by the shared dialog boundary. ***/

#pragma once

#include "cpssim/application/file_dialog.hpp"
#include "cpssim/gui/application_state.hpp"

#include <filesystem>
#include <string>

namespace cpssim {

enum class ProjectWorkflowStatus {
    Applied,
    Cancelled,
    Failed,
};

struct ProjectWorkflowResult {
    ProjectWorkflowStatus status{ProjectWorkflowStatus::Cancelled};
    std::filesystem::path selected_path;
    std::string diagnostic;
};

ProjectWorkflowResult open_project_from_dialog(GuiApplicationState& state, FileDialog& dialogs,
                                               const std::filesystem::path& initial_directory,
                                               const ProjectRuntimeResolver& runtime_resolver = {});

ProjectWorkflowResult load_run_plan_from_dialog(GuiSimulationSession& session, FileDialog& dialogs,
                                                const std::filesystem::path& initial_directory);

ProjectWorkflowResult save_run_plan_from_dialog(GuiSimulationSession& session, FileDialog& dialogs,
                                                const std::filesystem::path& suggested_path);

} // namespace cpssim
