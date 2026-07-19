/***
 * File: src/cpssim/model/runtime_state.cpp
 * Purpose: Implement validated job transitions and Resource-owned execution
 *          intervals, completion expectations, and integer busy accounting.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Ready membership belongs to Scheduler. Resource changes only the job
 *        selected for its exclusive Running execution state.
 ***/

#include "cpssim/model/runtime_state.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace cpssim {

/***
 * Copies stable identity/timing into a new Ready job, initializes its full
 * remaining demand and start history, and rejects invalid timing relations.
 ***/
JobState::JobState(JobId id, TaskId task_id, ResourceId resource_id, Priority priority,
                   JobTiming timing)
    : id_{id}, task_id_{task_id}, resource_id_{resource_id}, priority_{priority},
      release_tick_{timing.release}, absolute_deadline_{timing.absolute_deadline},
      remaining_execution_{timing.execution}, lifecycle_{JobLifecycle::Ready}, has_started_{false} {
    if (release_tick_ < 0) {
        throw std::invalid_argument{"job release tick must not be negative"};
    }
    if (absolute_deadline_ <= release_tick_) {
        throw std::invalid_argument{"job absolute deadline must be after its release"};
    }
    if (remaining_execution_ <= 0) {
        throw std::invalid_argument{"job execution demand must be positive"};
    }
    if (remaining_execution_ > absolute_deadline_ - release_tick_) {
        throw std::invalid_argument{"job execution demand must not exceed its relative deadline"};
    }
    if (priority_ < 0) {
        throw std::invalid_argument{"job priority must not be negative"};
    }
}

/***
 * Records the one deadline outcome owned by this job. Completion at the same
 * tick has already run in the earlier queue phase and therefore returns false.
 ***/
bool JobState::mark_deadline_missed(Tick tick) {
    if (tick != absolute_deadline_) {
        throw std::invalid_argument{"deadline check must occur at the absolute deadline"};
    }
    if (lifecycle_ == JobLifecycle::Completed) {
        return false;
    }
    if (deadline_missed_) {
        throw std::logic_error{"job deadline miss has already been recorded"};
    }
    deadline_missed_ = true;
    return true;
}

/*** Initializes an idle resource with zero accounted busy ticks. ***/
Resource::Resource(ResourceSpec spec) : spec_{std::move(spec)} {}

/***
 * Protects every transition from operating on a job configured for another
 * resource.
 ***/
void Resource::require_matching_resource(const JobState& job) const {
    if (job.resource_id_ != id()) {
        throw std::logic_error{"job belongs to a different resource"};
    }
}

/***
 * Verifies the scheduler-selected job and records its Running lifecycle,
 * interval start, and overflow-safe completion expectation.
 ***/
void Resource::start_job(JobState& job, Tick tick) {
    require_matching_resource(job);
    if (tick < job.release_tick_) {
        throw std::invalid_argument{"job cannot start before its release"};
    }
    if (job.lifecycle_ != JobLifecycle::Ready) {
        throw std::logic_error{"only a ready job can be started"};
    }
    if (running_job_.has_value()) {
        throw std::logic_error{"resource already has a running job"};
    }
    if (job.remaining_execution_ > std::numeric_limits<Tick>::max() - tick) {
        throw std::overflow_error{"resource completion tick exceeds Tick range"};
    }

    running_job_ = job.identity();
    running_since_ = tick;
    expected_completion_tick_ = tick + job.remaining_execution_;
    job.lifecycle_ = JobLifecycle::Running;
    if (!job.has_started_) {
        job.has_started_ = true;
        job.first_start_tick_ = tick;
    }
}

/***
 * Accounts for the elapsed interval before stopping the incomplete job. Ready
 * queue insertion is deliberately left to the commanding Scheduler.
 ***/
void Resource::preempt_job(JobState& job, Tick tick) {
    require_matching_resource(job);
    if (job.lifecycle_ != JobLifecycle::Running) {
        throw std::logic_error{"only a running job can be preempted"};
    }
    const auto identity = job.identity();
    if (!running_job_.has_value() || running_job_.value() != identity) {
        throw std::logic_error{"job is not running on this resource"};
    }
    if (!running_since_.has_value()) {
        throw std::logic_error{"running resource has no execution interval start"};
    }
    if (tick < running_since_.value()) {
        throw std::invalid_argument{"resource preemption cannot move time backward"};
    }
    const Tick elapsed = tick - running_since_.value();
    if (elapsed >= job.remaining_execution_) {
        throw std::logic_error{"job completion must be processed before preemption"};
    }
    if (elapsed > 0) {
        charge_execution(job, tick);
    }

    running_job_.reset();
    running_since_.reset();
    expected_completion_tick_.reset();
    job.lifecycle_ = JobLifecycle::Ready;
    ++job.preemption_count_;
}

/***
 * Derives a positive execution charge from the resource interval, accumulates
 * busy ticks, and completes the job exactly at its expected tick when demand
 * reaches zero.
 ***/
bool Resource::charge_execution(JobState& job, Tick end_tick) {
    require_matching_resource(job);
    if (job.lifecycle_ != JobLifecycle::Running) {
        throw std::logic_error{"execution can be charged only to a running job"};
    }
    if (!running_job_.has_value() || running_job_.value() != job.identity()) {
        throw std::logic_error{"job is not running on this resource"};
    }
    if (!running_since_.has_value()) {
        throw std::logic_error{"running resource has no execution interval start"};
    }
    if (end_tick <= running_since_.value()) {
        throw std::invalid_argument{"execution charge must advance resource time"};
    }
    const Tick charged_ticks = end_tick - running_since_.value();
    if (charged_ticks > job.remaining_execution_) {
        throw std::logic_error{"charged execution exceeds the job's remaining demand"};
    }
    if (charged_ticks == job.remaining_execution_ &&
        (!expected_completion_tick_.has_value() || expected_completion_tick_.value() != end_tick)) {
        throw std::logic_error{"job completed outside its expected completion tick"};
    }

    job.remaining_execution_ -= charged_ticks;
    busy_ticks_ += charged_ticks;
    if (job.remaining_execution_ > 0) {
        running_since_ = end_tick;
        return false;
    }

    job.lifecycle_ = JobLifecycle::Completed;
    job.finish_tick_ = end_tick;
    running_job_.reset();
    running_since_.reset();
    expected_completion_tick_.reset();
    return true;
}

/*** Includes any active uncharged interval without mutating job progress. ***/
Tick Resource::busy_ticks_until(Tick observation_tick) const {
    if (observation_tick < 0) {
        throw std::invalid_argument{"resource observation tick must not be negative"};
    }

    Tick observed_busy = busy_ticks_;
    if (running_since_.has_value()) {
        if (observation_tick < running_since_.value()) {
            throw std::invalid_argument{"resource observation predates its active interval"};
        }
        observed_busy += observation_tick - running_since_.value();
    }
    if (observed_busy > observation_tick) {
        throw std::logic_error{"resource busy time exceeds the observation interval"};
    }
    return observed_busy;
}

/*** Derives idle ticks from integer busy accounting over the same interval. ***/
Tick Resource::idle_ticks_until(Tick observation_tick) const {
    return observation_tick - busy_ticks_until(observation_tick);
}

} // namespace cpssim
