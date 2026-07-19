/***
 * File: src/cpssim/gui/draft_run_plan.hpp
 * Purpose: Declare editable GUI run-plan values without references to active
 *          simulator runtime state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/model/run_plan.hpp"

#include <optional>
#include <vector>

namespace cpssim {

/*** Holds one optional resource choice for a configured task. ***/
struct DraftTaskAssignment {
    TaskId task_id;
    std::optional<ResourceId> resource_id;

    bool operator==(const DraftTaskAssignment&) const = default;
};

/*** Owns incomplete typed fields edited before an immutable plan is built. ***/
class RunPlanDraft {
  public:
    // Creates one explicitly unassigned draft entry per configured task.
    RunPlanDraft(const ExperimentConfig& config, Tick initial_stop_tick);

    Tick stop_tick() const { return stop_tick_; }
    SchedulingPolicyKind policy_kind() const { return policy_kind_; }
    const std::vector<DraftTaskAssignment>& assignments() const { return assignments_; }

    void set_stop_tick(Tick stop_tick) { stop_tick_ = stop_tick; }
    void set_policy_kind(SchedulingPolicyKind policy_kind) { policy_kind_ = policy_kind; }

    // Updates one known task choice; an empty resource restores unassigned state.
    void set_assignment(TaskId task_id, std::optional<ResourceId> resource_id);

    // Returns the selected resource for one known task, or no value if unset.
    std::optional<ResourceId> assignment(TaskId task_id) const;

    // Builds shared raw input and validates it against the supplied experiment.
    RunPlanBuildResult build(const ExperimentConfig& config) const;

    // Compares canonical typed values; invalid or unapplied drafts are dirty.
    bool dirty(const ExperimentConfig& config, const RunPlan* active_plan) const;

  private:
    Tick stop_tick_;
    SchedulingPolicyKind policy_kind_{SchedulingPolicyKind::FixedPriority};
    std::vector<DraftTaskAssignment> assignments_;
};

} // namespace cpssim
