/***
 * File: src/cpssim/kernel/scheduler.hpp
 * Purpose: Declare runtime job submission, Ready queues, dispatch, deadline,
 *          completion, and preemption coordination over managed resources.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Scheduler owns runtime scheduling mechanism and forwards read-only
 *        functional observations to its non-owned SchedulingPolicy.
 ***/

#pragma once

#include "cpssim/kernel/event_queue.hpp"
#include "cpssim/model/experiment_config.hpp"
#include "cpssim/model/runtime_state.hpp"
#include "cpssim/policy/scheduling_policy.hpp"

#include <cstddef>
#include <vector>

namespace cpssim {

/*** Owns one scheduling domain containing independent execution resources. ***/
class Scheduler {
  public:
    /***
     * Builds idle resources in stable ID order and retains the non-owned policy
     * reference. The policy must outlive this scheduler. Throws when stop_tick
     * is negative or no resources are supplied.
     ***/
    Scheduler(const std::vector<ResourceSpec>& resources, SchedulingSpec scheduling,
              SchedulingPolicy& policy, Tick stop_tick);

    /***
     * Forwards one immutable functional observation to policy-owned decision
     * state before scheduling at the same tick.
     ***/
    void observe(const FunctionalObservation& observation);

    /***
     * Takes ownership of one newly released Ready job, places its identity in
     * the assigned resource queue, and schedules its in-horizon deadline check.
     * Rejects duplicate identity, active same-task overlap, or unknown resource.
     ***/
    void submit(JobState job, EventQueue& event_queue);

    /***
     * Applies a valid expected completion event. Returns false for an obsolete
     * completion candidate and true when the job completes successfully.
     ***/
    bool process_completion(const Event& event);

    // Records an incomplete job at its deadline and reports whether it missed.
    bool process_deadline(const Event& event);

    /***
     * Invokes the policy independently for nonempty resource Ready queues in
     * ascending ResourceId order and schedules resulting lifecycle events.
     ***/
    void schedule(Tick tick, EventQueue& event_queue);

    // Returns the immutable scheduling assumptions used by this domain.
    const SchedulingSpec& scheduling() const { return scheduling_; }

    // Returns the immutable configured preemption behavior.
    PreemptionMode preemption_mode() const { return scheduling_.preemption_mode; }

    // Returns every scheduler-owned job as a read-only view.
    const std::vector<JobState>& jobs() const { return jobs_; }

    // Returns the number of managed resources.
    std::size_t resource_count() const { return resource_states_.size(); }

    // Returns one managed resource as a read-only view.
    const Resource& resource(ResourceId resource_id) const;

    // Returns one resource's Ready identities as a read-only view.
    const std::vector<JobIdentity>& ready_jobs(ResourceId resource_id) const;

  private:
    /*** Keeps scheduler-owned Ready membership beside its execution resource. ***/
    struct ResourceSchedulingState {
        Resource resource;
        std::vector<JobIdentity> ready_jobs;
    };

    // Finds mutable scheduler state for one resource.
    ResourceSchedulingState& resource_state(ResourceId resource_id);

    // Finds read-only scheduler state for one resource.
    const ResourceSchedulingState& resource_state(ResourceId resource_id) const;

    // Finds one mutable scheduler-owned job.
    JobState& find_job(JobIdentity identity);

    // Finds one read-only scheduler-owned job.
    const JobState& find_job(JobIdentity identity) const;

    // Reports whether one task already has an incomplete submitted job.
    bool has_active_job(TaskId task_id) const;

    // Starts or resumes one validated Ready identity on an idle resource.
    void start_selected_job(ResourceSchedulingState& state, JobIdentity identity, Tick tick,
                            EventQueue& event_queue);

    SchedulingSpec scheduling_;
    SchedulingPolicy& policy_;
    Tick stop_tick_;
    std::vector<ResourceSchedulingState> resource_states_;
    std::vector<JobState> jobs_;
};

} // namespace cpssim
