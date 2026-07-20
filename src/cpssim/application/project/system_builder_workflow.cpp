/*** Implement strong-guarantee system rebuilds over Goal 1 project ownership. ***/

#include "cpssim/application/project/system_builder_workflow.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace cpssim {
namespace {

const RunPlan& accepted_project_plan(const ProjectContext& project) {
    const auto* active = project.session().active_plan();
    return active != nullptr ? *active : project.default_run_plan();
}

RunPlanRequest plan_request(const RunPlan& plan) {
    return {.stop_tick = plan.stop_tick(),
            .policy_kind = plan.policy_kind(),
            .assignments = plan.assignments()};
}

RunPlanRequest plan_request(const RunPlan& plan,
                            const std::vector<DraftTaskAssignment>& assignments) {
    auto request = RunPlanRequest{
        .stop_tick = plan.stop_tick(), .policy_kind = plan.policy_kind(), .assignments = {}};
    request.assignments.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        if (assignment.resource_id.has_value()) {
            request.assignments.push_back(
                {.task_id = assignment.task_id, .resource_id = *assignment.resource_id});
        }
    }
    return request;
}

bool assignments_match(const RunPlan& plan, const std::vector<DraftTaskAssignment>& assignments) {
    if (plan.assignments().size() != assignments.size()) {
        return false;
    }
    return std::all_of(assignments.begin(), assignments.end(), [&plan](const auto& assignment) {
        const auto found = std::find_if(plan.assignments().begin(), plan.assignments().end(),
                                        [&assignment](const auto& candidate) {
                                            return candidate.task_id == assignment.task_id;
                                        });
        return found != plan.assignments().end() && assignment.resource_id.has_value() &&
               found->resource_id == *assignment.resource_id;
    });
}

} // namespace

SystemProjectRebuildResult
build_system_project_replacement(const ProjectContext& current, const EditableSystemDraft& draft,
                                 const std::vector<DraftTaskAssignment>* assignments) {
    SystemProjectRebuildResult result;
    auto system = draft.build();
    if (!system.config.has_value()) {
        result.system_diagnostics = std::move(system.diagnostics);
        result.diagnostic = "system draft is invalid";
        return result;
    }
    auto& system_config = system.config.value();

    const auto& accepted = accepted_project_plan(current);
    auto run_plan = build_run_plan(system_config, assignments == nullptr
                                                      ? plan_request(accepted)
                                                      : plan_request(accepted, *assignments));
    if (!run_plan.plan.has_value()) {
        result.run_plan_diagnostics = std::move(run_plan.diagnostics);
        result.diagnostic = "the active/default run plan is incompatible with the edited system";
        return result;
    }
    auto& accepted_replacement_plan = run_plan.plan.value();

    try {
        result.replacement =
            make_project_context(current.root(), current.metadata(), std::move(system_config),
                                 std::move(accepted_replacement_plan), current.workspace(),
                                 current.runtime_inputs(), current.workspace_diagnostic());
    } catch (const std::exception& error) {
        result.diagnostic = std::string{"replacement session construction failed: "} + error.what();
    }
    return result;
}

SystemProjectRebuildResult
apply_system_project_draft(GuiApplicationState& state, const EditableSystemDraft& draft,
                           const std::vector<DraftTaskAssignment>* assignments) {
    if (!state.has_active_project()) {
        return {.replacement = nullptr,
                .system_diagnostics = {},
                .run_plan_diagnostics = {},
                .diagnostic = "system changes require an active project"};
    }
    auto result = build_system_project_replacement(state.active_project(), draft, assignments);
    if (result.replacement != nullptr) {
        result.applied = true;
        state.replace_project(std::move(result.replacement));
    }
    return result;
}

ProjectTransitionResult
resolve_unapplied_system_changes(GuiApplicationState& state, const EditableSystemDraft* draft,
                                 UnappliedSystemDecision decision,
                                 const std::vector<DraftTaskAssignment>* assignments) {
    if (draft == nullptr || !state.has_active_project()) {
        return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
    }
    const auto assignments_dirty =
        assignments != nullptr &&
        !assignments_match(accepted_project_plan(state.active_project()), *assignments);
    if (!draft->dirty() && !assignments_dirty) {
        return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
    }
    if (decision == UnappliedSystemDecision::Cancel) {
        return {.status = ProjectTransitionStatus::Cancelled, .diagnostic = {}};
    }
    if (decision == UnappliedSystemDecision::Discard) {
        return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
    }

    auto replacement =
        build_system_project_replacement(state.active_project(), *draft, assignments);
    if (!replacement.valid()) {
        return {.status = ProjectTransitionStatus::Failed, .diagnostic = replacement.diagnostic};
    }
    try {
        save_project(*replacement.replacement);
        replacement.applied = true;
        state.replace_project(std::move(replacement.replacement));
        return {.status = ProjectTransitionStatus::Proceed, .diagnostic = {}};
    } catch (const std::exception& error) {
        return {.status = ProjectTransitionStatus::Failed, .diagnostic = error.what()};
    }
}

} // namespace cpssim
