/***
 * File: src/cpssim/model/experiment_config.hpp
 * Purpose: Declare the validated, immutable top-level experiment
 *          configuration consumed by future simulator construction.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The class owns copied/moved specifications and exposes read-only
 *        views so runtime code cannot mutate its configuration accidentally.
 ***/

#pragma once

#include "cpssim/model/specifications.hpp"
#include "cpssim/model/time.hpp"

#include <vector>

namespace cpssim {

/*** Owns validated immutable input to future simulator runtime construction. ***/
class ExperimentConfig {
  public:
    /***
     * Creates a complete experiment configuration and validates its tick
     * period, collection contents, uniqueness, and task-resource profiles.
     * Throws std::invalid_argument when any cross-record invariant is broken.
     ***/
    ExperimentConfig(PhysicalDuration tick_period, SchedulingSpec scheduling,
                     std::vector<ResourceSpec> resources, std::vector<TaskSpec> tasks,
                     std::vector<TaskResourceProfile> task_resource_profiles,
                     std::vector<MessageRouteSpec> message_routes = {});

    // Returns the positive physical duration represented by one tick.
    PhysicalDuration tick_period() const { return tick_period_; }

    // Returns the scheduling assumptions shared by allocation and runtime.
    const SchedulingSpec& scheduling() const { return scheduling_; }

    // Returns the immutable collection of configured resources.
    const std::vector<ResourceSpec>& resources() const { return resources_; }

    // Returns the immutable collection of configured periodic tasks.
    const std::vector<TaskSpec>& tasks() const { return tasks_; }

    // Returns valid task-resource choices and their deterministic demands.
    const std::vector<TaskResourceProfile>& task_resource_profiles() const {
        return task_resource_profiles_;
    }

    // Returns completion-triggered task routes and their fixed timing.
    const std::vector<MessageRouteSpec>& message_routes() const { return message_routes_; }

  private:
    PhysicalDuration tick_period_;
    SchedulingSpec scheduling_;
    std::vector<ResourceSpec> resources_;
    std::vector<TaskSpec> tasks_;
    std::vector<TaskResourceProfile> task_resource_profiles_;
    std::vector<MessageRouteSpec> message_routes_;
};

} // namespace cpssim
