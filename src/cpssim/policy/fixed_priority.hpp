/***
 * File: src/cpssim/policy/fixed_priority.hpp
 * Purpose: Declare the deterministic fixed-priority SchedulingPolicy used by
 *          the baseline runtime Scheduler.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The policy selects identities only. It never changes Resource,
 *        JobState, EventQueue, or simulation time.
 ***/

#pragma once

#include "cpssim/policy/scheduling_policy.hpp"

namespace cpssim {

/*** Schedules jobs by the deterministic MATLAB fixed-priority ordering. ***/
class FixedPriorityPolicy : public SchedulingPolicy {
  public:
    /***
     * Returns the best identity from the resource's nonempty ready list.
     * Ordering is priority, release tick, task ID, then job ID. Throws
     * std::logic_error when ready membership and the job store disagree.
     ***/
    JobIdentity select(const Resource& resource, const std::vector<JobIdentity>& ready_jobs,
                       const std::vector<JobState>& jobs) const override;

    /***
     * Reports whether the best ready job has a strictly higher priority than
     * the running job. Equal priority never causes preemption.
     ***/
    bool should_preempt(const JobState& running_job, const JobState& ready_job) const override;
};

} // namespace cpssim
