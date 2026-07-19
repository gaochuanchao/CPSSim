/***
 * File: src/cpssim/kernel/periodic_release.cpp
 * Purpose: Implement task-owned assignment, runtime job generation, and
 *          incremental periodic releases.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Jobs capture an assignment when their release event is scheduled.
 *        Reassignment never migrates an already pending or released job.
 ***/

#include "cpssim/kernel/periodic_release.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cpssim {

/***
 * Stores the immutable task and its valid task-resource choices, initializes
 * job numbering at one, and validates direct construction outside a complete
 * ExperimentConfig.
 ***/
Task::Task(TaskSpec spec, std::vector<TaskResourceProfile> resource_profiles, Tick stop_tick)
    : spec_{std::move(spec)}, resource_profiles_{std::move(resource_profiles)},
      stop_tick_{stop_tick}, scheduled_tick_{spec_.offset()} {
    if (stop_tick_ < 0) {
        throw std::invalid_argument{"periodic release stop tick must not be negative"};
    }
    if (resource_profiles_.empty()) {
        throw std::invalid_argument{"runtime task must have an accessible resource"};
    }
    for (auto current = resource_profiles_.begin(); current != resource_profiles_.end();
         ++current) {
        if (current->task_id != spec_.id()) {
            throw std::invalid_argument{"runtime task received another task's resource profile"};
        }
        if (current->execution_time <= 0 || current->execution_time > spec_.deadline()) {
            throw std::invalid_argument{"runtime task received an invalid execution time"};
        }
        for (auto other = resource_profiles_.begin(); other != current; ++other) {
            if (current->resource_id == other->resource_id) {
                throw std::invalid_argument{"runtime task resource profiles must be unique"};
            }
        }
    }
}

/*** Validates and stores the resource choice used by future job releases. ***/
void Task::assign_resource(ResourceId resource_id) {
    execution_time_on(resource_id);
    assigned_resource_ = resource_id;
}

/*** Looks up deterministic demand for one accessible resource. ***/
Tick Task::execution_time_on(ResourceId resource_id) const {
    for (const auto& profile : resource_profiles_) {
        if (profile.resource_id == resource_id) {
            return profile.execution_time;
        }
    }
    throw std::invalid_argument{"resource is not accessible to this task"};
}

/***
 * Inserts the current release using the task's assignment and records that
 * exact resource as part of the pending job. State changes only after the
 * queue accepts the event.
 ***/
void Task::schedule_current_release(EventQueue& queue) {
    if (!assigned_resource_.has_value()) {
        throw std::logic_error{"task must be assigned before scheduling a release"};
    }
    const auto resource_id = assigned_resource_.value();
    queue.schedule(scheduled_tick_, EventPhase::JobRelease, EventType::JobRelease,
                   {.task_id = spec_.id(),
                    .job_id = scheduled_job_id_,
                    .resource_id = resource_id,
                    .message_id = std::nullopt,
                    .vehicle_id = std::nullopt});
    pending_resource_ = resource_id;
    release_pending_ = true;
}

/*** Schedules at most the first in-horizon release for this task. ***/
bool Task::schedule_initial_release(EventQueue& queue) {
    if (initialized_) {
        throw std::logic_error{"initial task release has already been scheduled"};
    }
    initialized_ = true;
    if (scheduled_tick_ > stop_tick_) {
        return false;
    }
    schedule_current_release(queue);
    return true;
}

/***
 * Validates the processed event against this task's one pending release,
 * creates its concrete job from the captured assignment, and advances by
 * exactly one period when a successor fits in the horizon.
 ***/
JobState Task::release(const Event& processed_release, EventQueue& queue) {
    if (!initialized_) {
        throw std::logic_error{"initial task release has not been scheduled"};
    }
    if (processed_release.phase() != EventPhase::JobRelease ||
        processed_release.type() != EventType::JobRelease) {
        throw std::logic_error{"task received a non-release event"};
    }

    const auto& entities = processed_release.entities();
    if (!entities.task_id.has_value() || !entities.job_id.has_value() ||
        !entities.resource_id.has_value()) {
        throw std::logic_error{"periodic release lacks task, job, or resource identity"};
    }
    if (!release_pending_ || processed_release.tick() != scheduled_tick_ ||
        entities.task_id.value() != spec_.id() || entities.job_id.value() != scheduled_job_id_ ||
        !pending_resource_.has_value() ||
        entities.resource_id.value() != pending_resource_.value()) {
        throw std::logic_error{"release event does not match the task's pending job"};
    }
    if (spec_.deadline() > std::numeric_limits<Tick>::max() - scheduled_tick_) {
        throw std::overflow_error{"job absolute deadline exceeds the Tick domain"};
    }

    JobState job{scheduled_job_id_, spec_.id(), pending_resource_.value(), spec_.priority(),
                 JobTiming{.release = scheduled_tick_,
                           .absolute_deadline = scheduled_tick_ + spec_.deadline(),
                           .execution = execution_time_on(pending_resource_.value())}};

    if (spec_.period() > stop_tick_ - scheduled_tick_) {
        release_pending_ = false;
        pending_resource_.reset();
        return job;
    }
    if (scheduled_job_id_.value() == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"periodic task job ID space exhausted"};
    }
    if (!assigned_resource_.has_value()) {
        throw std::logic_error{"task must be assigned before scheduling a release"};
    }

    const Tick next_tick = scheduled_tick_ + spec_.period();
    const JobId next_job_id{scheduled_job_id_.value() + 1};
    const auto next_resource = assigned_resource_.value();
    queue.schedule(next_tick, EventPhase::JobRelease, EventType::JobRelease,
                   {.task_id = spec_.id(),
                    .job_id = next_job_id,
                    .resource_id = next_resource,
                    .message_id = std::nullopt,
                    .vehicle_id = std::nullopt});

    scheduled_tick_ = next_tick;
    scheduled_job_id_ = next_job_id;
    pending_resource_ = next_resource;
    release_pending_ = true;
    return job;
}

/***
 * Builds each runtime Task with the subset of profiles bearing its task ID,
 * then orders tasks by first release, priority, and stable task ID.
 ***/
PeriodicReleaseModel::PeriodicReleaseModel(const ExperimentConfig& config, Tick stop_tick)
    : stop_tick_{stop_tick} {
    if (stop_tick_ < 0) {
        throw std::invalid_argument{"periodic release stop tick must not be negative"};
    }

    for (const auto& task_spec : config.tasks()) {
        std::vector<TaskResourceProfile> profiles;
        for (const auto& profile : config.task_resource_profiles()) {
            if (profile.task_id == task_spec.id()) {
                profiles.push_back(profile);
            }
        }
        tasks_.emplace_back(task_spec, std::move(profiles), stop_tick_);
    }

    std::sort(tasks_.begin(), tasks_.end(), [](const Task& left, const Task& right) {
        if (left.spec().offset() != right.spec().offset()) {
            return left.spec().offset() < right.spec().offset();
        }
        if (left.spec().priority() != right.spec().priority()) {
            return left.spec().priority() < right.spec().priority();
        }
        return left.spec().id() < right.spec().id();
    });
}

/*** Finds a runtime task or rejects an unknown task ID. ***/
Task& PeriodicReleaseModel::task_for(TaskId task_id) {
    const auto found = std::find_if(tasks_.begin(), tasks_.end(), [task_id](const Task& task) {
        return task.spec().id() == task_id;
    });
    if (found == tasks_.end()) {
        throw std::logic_error{"unknown runtime task"};
    }
    return *found;
}

/*** Applies an external policy/engine assignment without choosing it here. ***/
void PeriodicReleaseModel::assign_resource(TaskId task_id, ResourceId resource_id) {
    task_for(task_id).assign_resource(resource_id);
}

/*** Returns a read-only runtime task view. ***/
const Task& PeriodicReleaseModel::task(TaskId task_id) const {
    for (const auto& runtime_task : tasks_) {
        if (runtime_task.spec().id() == task_id) {
            return runtime_task;
        }
    }
    throw std::logic_error{"unknown runtime task"};
}

/***
 * Prevalidates all in-horizon assignments to avoid partial initialization,
 * then asks each Task to insert only its first release.
 ***/
std::size_t PeriodicReleaseModel::schedule_initial_releases(EventQueue& queue) {
    if (initialized_) {
        throw std::logic_error{"initial periodic releases have already been scheduled"};
    }
    for (const auto& runtime_task : tasks_) {
        if (runtime_task.spec().offset() <= stop_tick_ && !runtime_task.has_assignment()) {
            throw std::logic_error{"every releasing task must be assigned a resource"};
        }
    }

    initialized_ = true;
    std::size_t scheduled_count = 0;
    for (auto& runtime_task : tasks_) {
        if (runtime_task.schedule_initial_release(queue)) {
            ++scheduled_count;
        }
    }
    return scheduled_count;
}

/*** Finds the producing Task and delegates job generation and advancement. ***/
JobState PeriodicReleaseModel::release(const Event& processed_release, EventQueue& queue) {
    if (!initialized_) {
        throw std::logic_error{"initial periodic releases have not been scheduled"};
    }
    const auto task_id = processed_release.entities().task_id;
    if (task_id.has_value()) {
        return task_for(task_id.value()).release(processed_release, queue);
    }
    throw std::logic_error{"periodic release lacks a task identity"};
}

} // namespace cpssim
