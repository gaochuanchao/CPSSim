/***
 * File: src/cpssim/gui/draft_run_plan.cpp
 * Purpose: Implement detached typed run-plan editing and shared-plan building.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/draft_run_plan.hpp"

#include <algorithm>
#include <stdexcept>

namespace cpssim {

RunPlanDraft::RunPlanDraft(const ExperimentConfig& config, Tick initial_stop_tick)
    : stop_tick_{initial_stop_tick} {
    assignments_.reserve(config.tasks().size());
    for (const auto& task : config.tasks()) {
        assignments_.push_back({.task_id = task.id(), .resource_id = std::nullopt});
    }
}

void RunPlanDraft::set_assignment(TaskId task_id, std::optional<ResourceId> resource_id) {
    const auto found = std::find_if(
        assignments_.begin(), assignments_.end(),
        [task_id](const DraftTaskAssignment& assignment) { return assignment.task_id == task_id; });
    if (found == assignments_.end()) {
        throw std::invalid_argument{"run-plan draft refers to an unknown task"};
    }
    found->resource_id = resource_id;
}

std::optional<ResourceId> RunPlanDraft::assignment(TaskId task_id) const {
    const auto found = std::find_if(
        assignments_.begin(), assignments_.end(),
        [task_id](const DraftTaskAssignment& candidate) { return candidate.task_id == task_id; });
    if (found == assignments_.end()) {
        throw std::invalid_argument{"run-plan draft refers to an unknown task"};
    }
    return found->resource_id;
}

RunPlanBuildResult RunPlanDraft::build(const ExperimentConfig& config) const {
    RunPlanRequest request{.stop_tick = stop_tick_, .policy_kind = policy_kind_, .assignments = {}};
    request.assignments.reserve(assignments_.size());
    for (const auto& assignment_value : assignments_) {
        if (assignment_value.resource_id.has_value()) {
            request.assignments.push_back({.task_id = assignment_value.task_id,
                                           .resource_id = *assignment_value.resource_id});
        }
    }
    return build_run_plan(config, request);
}

bool RunPlanDraft::dirty(const ExperimentConfig& config, const RunPlan* active_plan) const {
    if (active_plan == nullptr) {
        return true;
    }
    const auto result = build(config);
    if (!result.plan.has_value() || !result.diagnostics.empty()) {
        return true;
    }
    return result.plan.value() != *active_plan;
}

} // namespace cpssim
