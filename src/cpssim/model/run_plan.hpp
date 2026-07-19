/***
 * File: src/cpssim/model/run_plan.hpp
 * Purpose: Declare immutable per-run simulation input and shared validation
 *          diagnostics for GUI and future CLI construction.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/model/experiment_config.hpp"
#include "cpssim/policy/resource_allocator.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim {

/*** Identifies the scheduling policy constructed for one simulation run. ***/
enum class SchedulingPolicyKind {
    FixedPriority,
};

/*** Categorizes actionable failures found before runtime construction. ***/
enum class RunPlanDiagnosticCode {
    MissingTaskAssignment,
    DuplicateTaskAssignment,
    UnknownTask,
    UnknownResource,
    InaccessibleResource,
    InvalidStopTick,
    UnsupportedPolicy,
    RunConstructionFailed,
};

/*** Addresses one validation message to a plan field or assignment pair. ***/
struct RunPlanDiagnostic {
    RunPlanDiagnosticCode code;
    std::optional<TaskId> task_id;
    std::optional<ResourceId> resource_id;
    std::string message;

    bool operator==(const RunPlanDiagnostic&) const = default;
};

/*** Holds possibly invalid caller input for the shared run-plan builder. ***/
struct RunPlanRequest {
    Tick stop_tick{0};
    SchedulingPolicyKind policy_kind{SchedulingPolicyKind::FixedPriority};
    std::vector<TaskAssignment> assignments;

    bool operator==(const RunPlanRequest&) const = default;
};

struct RunPlanBuildResult;

/*** Owns validated immutable choices used to construct and reset one run. ***/
class RunPlan {
  public:
    Tick stop_tick() const { return stop_tick_; }
    SchedulingPolicyKind policy_kind() const { return policy_kind_; }
    const std::vector<TaskAssignment>& assignments() const { return assignments_; }

    bool operator==(const RunPlan&) const = default;

  private:
    RunPlan(Tick stop_tick, SchedulingPolicyKind policy_kind,
            std::vector<TaskAssignment> assignments);

    friend RunPlanBuildResult build_run_plan(const ExperimentConfig& config,
                                             const RunPlanRequest& request);

    Tick stop_tick_;
    SchedulingPolicyKind policy_kind_;
    std::vector<TaskAssignment> assignments_;
};

/*** Returns either one accepted plan or complete deterministic diagnostics. ***/
struct RunPlanBuildResult {
    std::optional<RunPlan> plan;
    std::vector<RunPlanDiagnostic> diagnostics;

    bool valid() const { return plan.has_value(); }
};

/***
 * Validates a raw request against one immutable experiment and canonicalizes
 * accepted assignments into configured task order without constructing runtime
 * state.
 ***/
RunPlanBuildResult build_run_plan(const ExperimentConfig& config, const RunPlanRequest& request);

} // namespace cpssim
