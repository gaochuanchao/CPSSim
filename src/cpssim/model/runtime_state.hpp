/***
 * File: src/cpssim/model/runtime_state.hpp
 * Purpose: Declare mutable job records and execution-resource state with
 *          validated lifecycle and integer execution accounting.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Resource owns Running execution state but not Ready queues or policy.
 *        Scheduler owns Ready membership and commands these transitions.
 ***/

#pragma once

#include "cpssim/model/categories.hpp"
#include "cpssim/model/identifiers.hpp"
#include "cpssim/model/specifications.hpp"
#include "cpssim/model/time.hpp"

#include <cstddef>
#include <optional>

namespace cpssim {

class Resource;

/*** Groups named construction-time job values that all use Tick. ***/
struct JobTiming {
    Tick release;
    Tick absolute_deadline;
    Tick execution;
};

/*** Stores the mutable execution state of one released job instance. ***/
class JobState {
  public:
    /***
     * Creates a new Ready, never-started job with validated timing.
     * Throws std::invalid_argument for negative release, invalid deadline, or
     * nonpositive/excessive execution demand.
     ***/
    JobState(JobId id, TaskId task_id, ResourceId resource_id, Priority priority, JobTiming timing);

    // Returns this job's number within its producing task.
    JobId id() const { return id_; }

    // Returns the immutable task identifier that produced this job.
    TaskId task_id() const { return task_id_; }

    // Returns the complete task-and-job identity used by runtime collections.
    JobIdentity identity() const { return JobIdentity{task_id_, id_}; }

    // Returns the resource assignment inherited from the producing task.
    ResourceId resource_id() const { return resource_id_; }

    // Returns the fixed priority inherited from the producing task.
    Priority priority() const { return priority_; }

    // Returns the job's absolute release tick.
    Tick release_tick() const { return release_tick_; }

    // Returns the job's absolute deadline tick.
    Tick absolute_deadline() const { return absolute_deadline_; }

    // Returns the execution demand that has not yet been charged.
    Tick remaining_execution() const { return remaining_execution_; }

    // Returns the job's current mutually exclusive lifecycle state.
    JobLifecycle lifecycle() const { return lifecycle_; }

    // Reports whether the job has started at least once.
    bool has_started() const { return has_started_; }

    // Returns the first start tick, or no value before the job starts.
    std::optional<Tick> first_start_tick() const { return first_start_tick_; }

    // Returns the completion tick, or no value before completion.
    std::optional<Tick> finish_tick() const { return finish_tick_; }

    // Returns how many times the running job has been preempted.
    std::size_t preemption_count() const { return preemption_count_; }

    // Reports whether the incomplete job reached its absolute deadline.
    bool deadline_missed() const { return deadline_missed_; }

    /***
     * Records a deadline miss exactly at this job's absolute deadline.
     * Returns false for a job already completed on time and throws for a
     * wrong tick or repeated miss recording.
     ***/
    bool mark_deadline_missed(Tick tick);

  private:
    friend class Resource;

    JobId id_;
    TaskId task_id_;
    ResourceId resource_id_;
    Priority priority_;
    Tick release_tick_;
    Tick absolute_deadline_;
    Tick remaining_execution_;
    JobLifecycle lifecycle_;
    bool has_started_;
    std::optional<Tick> first_start_tick_;
    std::optional<Tick> finish_tick_;
    std::size_t preemption_count_{0};
    bool deadline_missed_{false};
};

/*** Executes at most one job and accounts for its integer busy time. ***/
class Resource {
  public:
    // Creates an idle runtime resource from its immutable specification.
    Resource(ResourceSpec spec);

    // Returns this resource's stable identifier.
    ResourceId id() const { return spec_.id(); }

    // Returns the immutable description used to construct this resource.
    const ResourceSpec& spec() const { return spec_; }

    // Returns the complete running-job identity, or no value while idle.
    std::optional<JobIdentity> running_job() const { return running_job_; }

    // Returns the start of the active execution interval, if any.
    std::optional<Tick> running_since() const { return running_since_; }

    // Returns the active job's expected completion candidate, if any.
    std::optional<Tick> expected_completion_tick() const { return expected_completion_tick_; }

    /***
     * Starts or resumes a scheduler-selected Ready job while this resource is
     * idle. Records its interval and expected completion. Throws when the job
     * does not belong here or a lifecycle/time invariant is broken.
     ***/
    void start_job(JobState& job, Tick tick);

    /***
     * Charges execution through tick, stops the incomplete Running job, and
     * marks it Ready for scheduler requeueing. Throws when completion should
     * already have been processed or runtime state is inconsistent.
     ***/
    void preempt_job(JobState& job, Tick tick);

    /***
     * Charges the active interval through end_tick. Returns true only when the
     * charge completes the job and idles this resource. Throws for nonpositive
     * elapsed execution, overcharging, or inconsistent runtime state.
     ***/
    bool charge_execution(JobState& job, Tick end_tick);

    /***
     * Returns busy ticks over [0, observation_tick), including an uncharged
     * active interval. Throws when the observation predates known state.
     ***/
    Tick busy_ticks_until(Tick observation_tick) const;

    // Returns nonbusy ticks over [0, observation_tick).
    Tick idle_ticks_until(Tick observation_tick) const;

  private:
    // Throws std::logic_error unless the job belongs to this resource.
    void require_matching_resource(const JobState& job) const;

    ResourceSpec spec_;
    std::optional<JobIdentity> running_job_;
    std::optional<Tick> running_since_;
    std::optional<Tick> expected_completion_tick_;
    Tick busy_ticks_{0};
};

} // namespace cpssim
