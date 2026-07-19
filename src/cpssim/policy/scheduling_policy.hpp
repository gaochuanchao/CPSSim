/***
 * File: src/cpssim/policy/scheduling_policy.hpp
 * Purpose: Declare the decision interface used by runtime Scheduler, including
 *          read-only functional observations and job-ranking decisions.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: A policy may update its private decision state from observations but
 *        never changes jobs, resources, events, model state, or time.
 ***/

#pragma once

#include "cpssim/functional/functional_model.hpp"
#include "cpssim/model/runtime_state.hpp"

#include <vector>

namespace cpssim {

/*** Defines the replaceable decisions used by runtime scheduling mechanism. ***/
class SchedulingPolicy {
  public:
    // Enables safe destruction through this base interface.
    virtual ~SchedulingPolicy() = default;

    /***
     * Receives functional observations in increasing tick order before any
     * scheduling choice at that tick. The default keeps context-free policies
     * unchanged. Implementations may update only their private policy state.
     ***/
    virtual void observe(const FunctionalObservation&) {}

    /***
     * Returns one identity from the nonempty Ready view for this resource.
     * Throws std::logic_error when supplied read-only views are inconsistent.
     ***/
    virtual JobIdentity select(const Resource& resource, const std::vector<JobIdentity>& ready_jobs,
                               const std::vector<JobState>& jobs) const = 0;

    /***
     * Reports whether the selected Ready job outranks the Running job. The
     * runtime Scheduler separately enforces configured preemption mode.
     ***/
    virtual bool should_preempt(const JobState& running_job, const JobState& ready_job) const = 0;
};

} // namespace cpssim
