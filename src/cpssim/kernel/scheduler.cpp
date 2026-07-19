/***
 * File: src/cpssim/kernel/scheduler.cpp
 * Purpose: Implement runtime scheduling mechanism independently of global
 *          simulation time advancement and canonical trace ownership.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Ready membership and jobs belong here; Resource performs only the
 *        commanded Running execution transitions and integer accounting.
 ***/

#include "cpssim/kernel/scheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace cpssim {

namespace {

/*** Creates canonical entity references for one scheduler-owned job. ***/
EventEntityRefs job_entities(const JobState& job) {
    return {.task_id = job.task_id(),
            .job_id = job.id(),
            .resource_id = job.resource_id(),
            .message_id = std::nullopt,
            .vehicle_id = std::nullopt};
}

/*** Extracts the complete identity required by every scheduler job event. ***/
JobIdentity event_job_identity(const Event& event) {
    const auto task_id = event.entities().task_id;
    const auto job_id = event.entities().job_id;
    if (!task_id.has_value() || !job_id.has_value()) {
        throw std::logic_error{"scheduler job event lacks task or job identity"};
    }
    return JobIdentity{*task_id, *job_id};
}

} // namespace

/*** Creates and canonically orders the exclusive runtime resources. ***/
Scheduler::Scheduler(const std::vector<ResourceSpec>& resources, SchedulingSpec scheduling,
                     SchedulingPolicy& policy, Tick stop_tick)
    : scheduling_{scheduling}, policy_{policy}, stop_tick_{stop_tick} {
    if (stop_tick_ < 0) {
        throw std::invalid_argument{"scheduler stop tick must not be negative"};
    }
    if (resources.empty()) {
        throw std::invalid_argument{"scheduler requires at least one resource"};
    }

    resource_states_.reserve(resources.size());
    for (const auto& resource_spec : resources) {
        resource_states_.push_back(
            {.resource = Resource{resource_spec}, .ready_jobs = std::vector<JobIdentity>{}});
    }
    std::sort(resource_states_.begin(), resource_states_.end(),
              [](const ResourceSchedulingState& left, const ResourceSchedulingState& right) {
                  return left.resource.id() < right.resource.id();
              });
    for (std::size_t index = 1; index < resource_states_.size(); ++index) {
        if (resource_states_[index - 1].resource.id() == resource_states_[index].resource.id()) {
            throw std::invalid_argument{"scheduler resource IDs must be unique"};
        }
    }
}

/*** Delegates observation interpretation without exposing scheduler state. ***/
void Scheduler::observe(const FunctionalObservation& observation) { policy_.observe(observation); }

/*** Finds mutable state without exposing scheduler-owned Ready membership. ***/
Scheduler::ResourceSchedulingState& Scheduler::resource_state(ResourceId resource_id) {
    for (auto& state : resource_states_) {
        if (state.resource.id() == resource_id) {
            return state;
        }
    }
    throw std::logic_error{"runtime resource identity is absent from the scheduler"};
}

/*** Finds read-only state for public inspection and policy calls. ***/
const Scheduler::ResourceSchedulingState& Scheduler::resource_state(ResourceId resource_id) const {
    for (const auto& state : resource_states_) {
        if (state.resource.id() == resource_id) {
            return state;
        }
    }
    throw std::logic_error{"runtime resource identity is absent from the scheduler"};
}

/*** Returns a managed resource without exposing mutable execution state. ***/
const Resource& Scheduler::resource(ResourceId resource_id) const {
    return resource_state(resource_id).resource;
}

/*** Returns Ready membership owned by the scheduler for one resource. ***/
const std::vector<JobIdentity>& Scheduler::ready_jobs(ResourceId resource_id) const {
    return resource_state(resource_id).ready_jobs;
}

/*** Resolves one mutable job by its task-local complete identity. ***/
JobState& Scheduler::find_job(JobIdentity identity) {
    for (auto& job : jobs_) {
        if (job.identity() == identity) {
            return job;
        }
    }
    throw std::logic_error{"runtime job identity is absent from the scheduler store"};
}

/*** Resolves one read-only job by its task-local complete identity. ***/
const JobState& Scheduler::find_job(JobIdentity identity) const {
    for (const auto& job : jobs_) {
        if (job.identity() == identity) {
            return job;
        }
    }
    throw std::logic_error{"runtime job identity is absent from the scheduler store"};
}

/*** Searches for unsupported same-task overlap among nonterminal jobs. ***/
bool Scheduler::has_active_job(TaskId task_id) const {
    for (const auto& job : jobs_) {
        if (job.task_id() == task_id && job.lifecycle() != JobLifecycle::Completed &&
            job.lifecycle() != JobLifecycle::Cancelled) {
            return true;
        }
    }
    return false;
}

/*** Validates submission before transferring ownership and scheduling deadline. ***/
void Scheduler::submit(JobState job, EventQueue& event_queue) {
    if (job.lifecycle() != JobLifecycle::Ready || job.has_started()) {
        throw std::logic_error{"scheduler accepts only a newly released Ready job"};
    }
    if (job.release_tick() > stop_tick_) {
        throw std::invalid_argument{"scheduler cannot submit a job beyond its run horizon"};
    }
    if (has_active_job(job.task_id())) {
        throw std::logic_error{"periodic task submitted a self-overlapping job"};
    }
    for (const auto& stored_job : jobs_) {
        if (stored_job.identity() == job.identity()) {
            throw std::logic_error{"scheduler received a duplicate job identity"};
        }
    }

    auto& state = resource_state(job.resource_id());
    const auto identity = job.identity();
    if (job.absolute_deadline() <= stop_tick_) {
        event_queue.schedule(job.absolute_deadline(), EventPhase::DeadlineCheck,
                             EventType::DeadlineMiss, job_entities(job));
    }
    jobs_.push_back(job);
    state.ready_jobs.push_back(identity);
}

/*** Rejects unsupported shapes and ignores only deterministic stale candidates. ***/
bool Scheduler::process_completion(const Event& event) {
    const auto resource_id = event.entities().resource_id;
    if (event.phase() != EventPhase::ExecutionCompletion || event.type() != EventType::JobFinish ||
        !resource_id.has_value()) {
        throw std::logic_error{"scheduler received an invalid completion event"};
    }

    const auto identity = event_job_identity(event);
    auto& state = resource_state(*resource_id);
    const auto running_job = state.resource.running_job();
    const auto completion_tick = state.resource.expected_completion_tick();
    if (!running_job.has_value() || *running_job != identity || !completion_tick.has_value() ||
        *completion_tick != event.tick()) {
        return false;
    }

    auto& job = find_job(identity);
    if (!state.resource.charge_execution(job, event.tick())) {
        throw std::logic_error{"expected completion left execution remaining"};
    }
    return true;
}

/*** Applies one deadline observation to the scheduler-owned job. ***/
bool Scheduler::process_deadline(const Event& event) {
    const auto resource_id = event.entities().resource_id;
    if (event.phase() != EventPhase::DeadlineCheck || event.type() != EventType::DeadlineMiss ||
        !resource_id.has_value()) {
        throw std::logic_error{"scheduler received an invalid deadline event"};
    }
    auto& job = find_job(event_job_identity(event));
    if (job.resource_id() != *resource_id) {
        throw std::logic_error{"deadline event resource does not match its job"};
    }
    return job.mark_deadline_missed(event.tick());
}

/*** Starts one policy-selected identity and creates its observable candidates. ***/
void Scheduler::start_selected_job(ResourceSchedulingState& state, JobIdentity identity, Tick tick,
                                   EventQueue& event_queue) {
    const auto ready_position =
        std::find(state.ready_jobs.begin(), state.ready_jobs.end(), identity);
    if (ready_position == state.ready_jobs.end()) {
        throw std::logic_error{"scheduling policy selected a job outside the Ready queue"};
    }

    auto& job = find_job(identity);
    if (job.resource_id() != state.resource.id() || job.lifecycle() != JobLifecycle::Ready) {
        throw std::logic_error{"scheduling policy selected an invalid Ready job"};
    }
    const bool is_resume = job.has_started();
    state.resource.start_job(job, tick);
    state.ready_jobs.erase(ready_position);
    event_queue.schedule(tick, EventPhase::Scheduling,
                         is_resume ? EventType::JobResume : EventType::JobStart, job_entities(job));

    const auto completion_tick = state.resource.expected_completion_tick();
    if (completion_tick.has_value() && *completion_tick <= stop_tick_) {
        event_queue.schedule(*completion_tick, EventPhase::ExecutionCompletion,
                             EventType::JobFinish, job_entities(job));
    }
}

/*** Applies one deterministic scheduling cycle across all managed resources. ***/
void Scheduler::schedule(Tick tick, EventQueue& event_queue) {
    for (auto& state : resource_states_) {
        if (state.ready_jobs.empty()) {
            continue;
        }
        const auto running_identity = state.resource.running_job();
        if (running_identity.has_value() &&
            scheduling_.preemption_mode == PreemptionMode::NonPreemptive) {
            continue;
        }

        const auto selected = policy_.select(state.resource, state.ready_jobs, jobs_);
        if (std::find(state.ready_jobs.begin(), state.ready_jobs.end(), selected) ==
            state.ready_jobs.end()) {
            throw std::logic_error{"scheduling policy selected a job outside the Ready queue"};
        }
        auto& selected_job = find_job(selected);
        if (selected_job.resource_id() != state.resource.id() ||
            selected_job.lifecycle() != JobLifecycle::Ready) {
            throw std::logic_error{"scheduling policy selected an invalid Ready job"};
        }
        if (!running_identity.has_value()) {
            start_selected_job(state, selected, tick, event_queue);
            continue;
        }
        auto& running_job = find_job(*running_identity);
        if (!policy_.should_preempt(running_job, selected_job)) {
            continue;
        }

        const auto preempted_identity = running_job.identity();
        const auto preempted_entities = job_entities(running_job);
        state.resource.preempt_job(running_job, tick);
        state.ready_jobs.push_back(preempted_identity);
        event_queue.schedule(tick, EventPhase::Scheduling, EventType::JobPreempt,
                             preempted_entities);
        start_selected_job(state, selected, tick, event_queue);
    }
}

} // namespace cpssim
