/***
 * File: src/cpssim/policy/resource_allocator.hpp
 * Purpose: Declare task-to-resource allocation decisions that occur before
 *          periodic jobs are released.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Allocation may read SchedulingSpec for schedulability analysis but
 *        does not mutate runtime Scheduler, jobs, tasks, or resources.
 ***/

#pragma once

#include "cpssim/model/experiment_config.hpp"

#include <vector>

namespace cpssim {

/*** Names one task-to-resource choice in a complete allocation plan. ***/
struct TaskAssignment {
    TaskId task_id;
    ResourceId resource_id;

    bool operator==(const TaskAssignment&) const = default;
};

/*** Defines how an experiment obtains its initial task placement. ***/
class ResourceAllocator {
  public:
    // Enables safe destruction through this base interface.
    virtual ~ResourceAllocator() = default;

    /***
     * Returns exactly one accessible resource assignment for every task.
     * The caller validates and applies the plan before scheduling releases.
     ***/
    virtual std::vector<TaskAssignment> allocate(const ExperimentConfig& config) const = 0;
};

/*** Assigns every task to the experiment's only configured resource. ***/
class SingleResourceAllocator : public ResourceAllocator {
  public:
    /***
     * Returns one assignment per task. Throws std::invalid_argument unless
     * exactly one resource exists and every task can execute on it.
     ***/
    std::vector<TaskAssignment> allocate(const ExperimentConfig& config) const override;
};

/*** Returns an explicit task placement supplied by an experiment runner. ***/
class ConfiguredResourceAllocator : public ResourceAllocator {
  public:
    /***
     * Copies a caller-provided assignment plan. SimulationEngine later checks
     * task coverage, uniqueness, resource existence, and accessibility.
     ***/
    ConfiguredResourceAllocator(const std::vector<TaskAssignment>& assignments);

    // Returns the stored plan without choosing or changing any assignment.
    std::vector<TaskAssignment> allocate(const ExperimentConfig& config) const override;

  private:
    std::vector<TaskAssignment> assignments_;
};

} // namespace cpssim
