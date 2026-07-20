/*** Keep cancellations and failures outside active application state. ***/

#include "cpssim/application/project/project_workflow.hpp"

#include "cpssim/config/json_run_plan.hpp"

#include <exception>
#include <stdexcept>

namespace cpssim {
namespace {

ProjectWorkflowResult dialog_failure(const FileDialogResult& selection) {
    if (selection.status == FileDialogStatus::Cancelled) {
        return {};
    }
    return {.status = ProjectWorkflowStatus::Failed,
            .selected_path = {},
            .diagnostic =
                selection.diagnostic.empty() ? "File dialog failed" : selection.diagnostic};
}

} // namespace

ProjectWorkflowResult open_project_from_dialog(GuiApplicationState& state, FileDialog& dialogs,
                                               const std::filesystem::path& initial_directory,
                                               const ProjectRuntimeResolver& runtime_resolver) {
    const auto selection = dialogs.open_project(initial_directory);
    if (selection.status != FileDialogStatus::Selected) {
        return dialog_failure(selection);
    }
    try {
        auto replacement = load_project(selection.path, runtime_resolver);
        state.replace_project(std::move(replacement));
        return {.status = ProjectWorkflowStatus::Applied,
                .selected_path = selection.path,
                .diagnostic = {}};
    } catch (const std::exception& error) {
        return {.status = ProjectWorkflowStatus::Failed,
                .selected_path = selection.path,
                .diagnostic = error.what()};
    }
}

ProjectWorkflowResult load_run_plan_from_dialog(GuiSimulationSession& session, FileDialog& dialogs,
                                                const std::filesystem::path& initial_directory) {
    const auto selection = dialogs.open_run_plan(initial_directory);
    if (selection.status != FileDialogStatus::Selected) {
        return dialog_failure(selection);
    }
    try {
        auto plan = load_run_plan(selection.path, session.config());
        if (!session.replace_draft(plan)) {
            throw std::runtime_error{"the run plan cannot be replaced while running"};
        }
        return {.status = ProjectWorkflowStatus::Applied,
                .selected_path = selection.path,
                .diagnostic = {}};
    } catch (const std::exception& error) {
        return {.status = ProjectWorkflowStatus::Failed,
                .selected_path = selection.path,
                .diagnostic = error.what()};
    }
}

ProjectWorkflowResult save_run_plan_from_dialog(GuiSimulationSession& session, FileDialog& dialogs,
                                                const std::filesystem::path& suggested_path) {
    const auto selection = dialogs.save_run_plan(suggested_path);
    if (selection.status != FileDialogStatus::Selected) {
        return dialog_failure(selection);
    }
    try {
        const auto& validation = session.validate_draft();
        if (!validation.plan.has_value() || !validation.diagnostics.empty()) {
            throw std::invalid_argument{"pending run plan is invalid"};
        }
        save_run_plan(selection.path, session.config(), validation.plan.value());
        return {.status = ProjectWorkflowStatus::Applied,
                .selected_path = selection.path,
                .diagnostic = {}};
    } catch (const std::exception& error) {
        return {.status = ProjectWorkflowStatus::Failed,
                .selected_path = selection.path,
                .diagnostic = error.what()};
    }
}

} // namespace cpssim
