/***
 * File: src/cpssim/kernel/periodic_release.hpp
 * Purpose: Declare runtime tasks that generate one job and one successor
 *          release at a time during simulation.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: A Task owns its current resource assignment and periodic-release
 *        position. Resource selection policy remains outside this module.
 ***/

#pragma once

#include "cpssim/kernel/event_queue.hpp"
#include "cpssim/model/experiment_config.hpp"
#include "cpssim/model/runtime_state.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace cpssim {

/*** Models the mutable runtime behavior of one configured periodic task. ***/
class Task {
  public:
    /***
     * Creates one runtime task from its immutable specification, accessible
     * resource profiles, and inclusive simulation stop tick.
     ***/
    Task(TaskSpec spec, std::vector<TaskResourceProfile> resource_profiles, Tick stop_tick);

    // Returns the immutable task description used at construction.
    const TaskSpec& spec() const { return spec_; }

    // Returns the accessible resources and execution time on each one.
    const std::vector<TaskResourceProfile>& resource_profiles() const { return resource_profiles_; }

    // Returns the currently assigned resource, if the engine assigned one.
    std::optional<ResourceId> assigned_resource() const { return assigned_resource_; }

    // Reports whether a resource has been assigned to this runtime task.
    bool has_assignment() const { return assigned_resource_.has_value(); }

    /***
     * Assigns one accessible resource to the task. Future jobs inherit this
     * choice; an already pending release keeps the resource it captured.
     ***/
    void assign_resource(ResourceId resource_id);

    /***
     * Returns this task's deterministic execution time on an accessible
     * resource. Throws std::invalid_argument when the resource is inaccessible.
     ***/
    Tick execution_time_on(ResourceId resource_id) const;

    /***
     * Schedules the first release when it lies inside the horizon. Returns
     * false when no release is in range and rejects repeated initialization.
     ***/
    bool schedule_initial_release(EventQueue& queue);

    /***
     * Converts this task's processed pending release into a Ready JobState.
     * The job captures the pending resource and its execution time. The task
     * then schedules only its next release, using its current assignment.
     ***/
    JobState release(const Event& processed_release, EventQueue& queue);

  private:
    // Schedules the currently stored release position with the current assignment.
    void schedule_current_release(EventQueue& queue);

    TaskSpec spec_;
    std::vector<TaskResourceProfile> resource_profiles_;
    Tick stop_tick_;
    Tick scheduled_tick_;
    JobId scheduled_job_id_{1};
    std::optional<ResourceId> assigned_resource_;
    std::optional<ResourceId> pending_resource_;
    bool initialized_{false};
    bool release_pending_{false};
};

/***
 * Owns all runtime tasks and provides the engine-facing assignment and
 * release operations. It coordinates deterministic initial insertion only;
 * each Task owns its own release sequence.
 ***/
class PeriodicReleaseModel {
  public:
    // Builds one runtime Task for every immutable task specification.
    PeriodicReleaseModel(const ExperimentConfig& config, Tick stop_tick);

    // Applies an allocator choice to one runtime task.
    void assign_resource(TaskId task_id, ResourceId resource_id);

    // Returns one runtime task as a read-only view.
    const Task& task(TaskId task_id) const;

    /***
     * Schedules each in-horizon task's first release. Every such task must
     * already have an assignment. Returns the number of inserted events.
     ***/
    std::size_t schedule_initial_releases(EventQueue& queue);

    // Delegates one processed release to its producing Task and returns its job.
    JobState release(const Event& processed_release, EventQueue& queue);

  private:
    // Finds a mutable runtime task or rejects an unknown task identifier.
    Task& task_for(TaskId task_id);

    std::vector<Task> tasks_;
    Tick stop_tick_;
    bool initialized_{false};
};

} // namespace cpssim
