/***
 * File: src/cpssim/model/run_plan.cpp
 * Purpose: Implement shared run-plan validation and canonical construction.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/model/run_plan.hpp"

#include <algorithm>
#include <utility>

namespace cpssim {
namespace {

bool contains_task(const ExperimentConfig& config, TaskId task_id) {
    return std::any_of(config.tasks().begin(), config.tasks().end(),
                       [task_id](const TaskSpec& task) { return task.id() == task_id; });
}

bool contains_resource(const ExperimentConfig& config, ResourceId resource_id) {
    return std::any_of(
        config.resources().begin(), config.resources().end(),
        [resource_id](const ResourceSpec& resource) { return resource.id() == resource_id; });
}

bool contains_profile(const ExperimentConfig& config, TaskId task_id, ResourceId resource_id) {
    return std::any_of(config.task_resource_profiles().begin(),
                       config.task_resource_profiles().end(),
                       [task_id, resource_id](const TaskResourceProfile& profile) {
                           return profile.task_id == task_id && profile.resource_id == resource_id;
                       });
}

void add_diagnostic(std::vector<RunPlanDiagnostic>& diagnostics, RunPlanDiagnosticCode code,
                    std::string message, std::optional<TaskId> task_id = std::nullopt,
                    std::optional<ResourceId> resource_id = std::nullopt) {
    diagnostics.push_back({.code = code,
                           .task_id = task_id,
                           .resource_id = resource_id,
                           .message = std::move(message)});
}

} // namespace

RunPlan::RunPlan(Tick stop_tick, SchedulingPolicyKind policy_kind,
                 std::vector<TaskAssignment> assignments)
    : stop_tick_{stop_tick}, policy_kind_{policy_kind}, assignments_{std::move(assignments)} {}

RunPlanBuildResult build_run_plan(const ExperimentConfig& config, const RunPlanRequest& request) {
    RunPlanBuildResult result;

    if (request.stop_tick < 0) {
        add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::InvalidStopTick,
                       "stop tick must not be negative");
    }
    switch (request.policy_kind) {
    case SchedulingPolicyKind::FixedPriority:
        break;
    default:
        add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::UnsupportedPolicy,
                       "scheduling policy is not supported");
        break;
    }

    auto ordered_input = request.assignments;
    std::sort(ordered_input.begin(), ordered_input.end(), [](const auto& left, const auto& right) {
        if (left.task_id != right.task_id) {
            return left.task_id < right.task_id;
        }
        return left.resource_id < right.resource_id;
    });
    for (const auto& assignment : ordered_input) {
        if (!contains_task(config, assignment.task_id)) {
            add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::UnknownTask,
                           "assignment refers to an unknown task", assignment.task_id,
                           assignment.resource_id);
        }
    }

    std::vector<TaskAssignment> canonical_assignments;
    canonical_assignments.reserve(config.tasks().size());
    for (const auto& task : config.tasks()) {
        const auto count = static_cast<std::size_t>(std::count_if(
            request.assignments.begin(), request.assignments.end(),
            [&task](const TaskAssignment& assignment) { return assignment.task_id == task.id(); }));
        if (count == 0) {
            add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::MissingTaskAssignment,
                           "task requires one resource assignment", task.id());
            continue;
        }
        if (count > 1) {
            add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::DuplicateTaskAssignment,
                           "task is assigned more than once", task.id());
            continue;
        }

        const auto assignment = *std::find_if(
            request.assignments.begin(), request.assignments.end(),
            [&task](const TaskAssignment& candidate) { return candidate.task_id == task.id(); });
        if (!contains_resource(config, assignment.resource_id)) {
            add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::UnknownResource,
                           "assignment refers to an unknown resource", assignment.task_id,
                           assignment.resource_id);
            continue;
        }
        if (!contains_profile(config, assignment.task_id, assignment.resource_id)) {
            add_diagnostic(result.diagnostics, RunPlanDiagnosticCode::InaccessibleResource,
                           "task has no execution profile for the selected resource",
                           assignment.task_id, assignment.resource_id);
            continue;
        }
        canonical_assignments.push_back(assignment);
    }

    if (result.diagnostics.empty()) {
        result.plan =
            RunPlan{request.stop_tick, request.policy_kind, std::move(canonical_assignments)};
    }
    return result;
}

} // namespace cpssim
