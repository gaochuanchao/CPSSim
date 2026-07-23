/***
 * File: src/cpssim/model/experiment_config.cpp
 * Purpose: Implement validation of complete experiment configurations across
 *          resource and task collections.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Cross-record checks belong here rather than in individual
 *        specifications because they require the complete experiment view.
 ***/

#include "cpssim/model/experiment_config.hpp"

#include <stdexcept>
#include <utility>

namespace cpssim {

namespace {

/***
 * Requires at least one resource and verifies that every resource ID and name
 * is unique. Throws std::invalid_argument on the first violation.
 ***/
void validate_resources(const std::vector<ResourceSpec>& resources) {
    if (resources.empty()) {
        throw std::invalid_argument{"experiment must define at least one resource"};
    }

    for (auto current = resources.begin(); current != resources.end(); ++current) {
        for (auto other = resources.begin(); other != current; ++other) {
            if (current->id() == other->id()) {
                throw std::invalid_argument{"resource IDs must be unique"};
            }
            if (current->name() == other->name()) {
                throw std::invalid_argument{"resource names must be unique"};
            }
        }
    }
}

/***
 * Requires at least one task and verifies unique task IDs and names.
 * Throws std::invalid_argument on the first violation.
 ***/
void validate_tasks(const std::vector<TaskSpec>& tasks) {
    if (tasks.empty()) {
        throw std::invalid_argument{"experiment must define at least one task"};
    }

    for (auto current = tasks.begin(); current != tasks.end(); ++current) {
        for (auto other = tasks.begin(); other != current; ++other) {
            if (current->id() == other->id()) {
                throw std::invalid_argument{"task IDs must be unique"};
            }
            if (current->name() == other->name()) {
                throw std::invalid_argument{"task names must be unique"};
            }
        }
    }
}

// Reports whether the supplied task ID is present in the configuration.
bool contains_task(const std::vector<TaskSpec>& tasks, TaskId id) {
    for (const auto& task : tasks) {
        if (task.id() == id) {
            return true;
        }
    }
    return false;
}

// Reports whether the supplied resource ID is present in the configuration.
bool contains_resource(const std::vector<ResourceSpec>& resources, ResourceId id) {
    for (const auto& resource : resources) {
        if (resource.id() == id) {
            return true;
        }
    }
    return false;
}

// Finds the relative deadline of an already validated task ID.
Tick task_deadline(const std::vector<TaskSpec>& tasks, TaskId id) {
    for (const auto& task : tasks) {
        if (task.id() == id) {
            return task.deadline();
        }
    }
    throw std::logic_error{"validated task profile lost its task"};
}

/***
 * Validates the many-to-many task-resource relation. Every task needs at
 * least one accessible resource, every pair must be unique and refer to
 * defined records, and execution demand must fit the task deadline.
 ***/
void validate_task_resource_profiles(const std::vector<TaskResourceProfile>& profiles,
                                     const std::vector<TaskSpec>& tasks,
                                     const std::vector<ResourceSpec>& resources) {
    if (profiles.empty()) {
        throw std::invalid_argument{"experiment must define task-resource profiles"};
    }

    for (auto current = profiles.begin(); current != profiles.end(); ++current) {
        if (!contains_task(tasks, current->task_id)) {
            throw std::invalid_argument{"task-resource profile refers to an unknown task"};
        }
        if (!contains_resource(resources, current->resource_id)) {
            throw std::invalid_argument{"task-resource profile refers to an unknown resource"};
        }
        if (current->execution_time <= 0) {
            throw std::invalid_argument{"task-resource execution time must be positive"};
        }
        if (current->execution_time > task_deadline(tasks, current->task_id)) {
            throw std::invalid_argument{
                "task-resource execution time must not exceed the task deadline"};
        }

        for (auto other = profiles.begin(); other != current; ++other) {
            if (current->task_id == other->task_id && current->resource_id == other->resource_id) {
                throw std::invalid_argument{"task-resource profiles must be unique"};
            }
        }
    }

    for (const auto& task : tasks) {
        auto has_profile = false;
        for (const auto& profile : profiles) {
            if (profile.task_id == task.id()) {
                has_profile = true;
                break;
            }
        }
        if (!has_profile) {
            throw std::invalid_argument{"every task must have an accessible resource"};
        }
    }
}

/***
 * Validates fixed message timing, endpoint references, and unique route pairs.
 * Empty routes preserve experiments that do not model communication.
 ***/
void validate_message_routes(const std::vector<MessageRouteSpec>& routes,
                             const std::vector<TaskSpec>& tasks) {
    for (auto current = routes.begin(); current != routes.end(); ++current) {
        if (!contains_task(tasks, current->source_task_id) ||
            !contains_task(tasks, current->destination_task_id)) {
            throw std::invalid_argument{"message route refers to an unknown task"};
        }
        // kind: 0=Communication, 1=Logical (int avoids GUI header dependency)
        if (current->kind != 0 && current->kind != 1) {
            throw std::invalid_argument{
                "message route kind must be Communication (0) or Logical (1)"};
        }
        if (current->send_offset != message_route_send_offset_ticks) {
            throw std::invalid_argument{
                "message route send offset must equal the fixed one-tick causal offset"};
        }
        if (current->delay <= 0) {
            throw std::invalid_argument{"message route delay must be positive"};
        }

        for (auto other = routes.begin(); other != current; ++other) {
            if (current->source_task_id == other->source_task_id &&
                current->destination_task_id == other->destination_task_id) {
                throw std::invalid_argument{"message route endpoint pairs must be unique"};
            }
        }
    }
}

} // namespace

/***
 * Takes ownership of the validated specification collections, rejects a
 * nonpositive tick period, and runs the cross-record resource/task checks.
 ***/
ExperimentConfig::ExperimentConfig(PhysicalDuration tick_period, SchedulingSpec scheduling,
                                   std::vector<ResourceSpec> resources, std::vector<TaskSpec> tasks,
                                   std::vector<TaskResourceProfile> task_resource_profiles,
                                   std::vector<MessageRouteSpec> message_routes)
    : tick_period_{tick_period}, scheduling_{scheduling}, resources_{std::move(resources)},
      tasks_{std::move(tasks)}, task_resource_profiles_{std::move(task_resource_profiles)},
      message_routes_{std::move(message_routes)} {
    if (tick_period_.count() <= 0) {
        throw std::invalid_argument{"experiment tick period must be positive"};
    }

    validate_resources(resources_);
    validate_tasks(tasks_);
    validate_task_resource_profiles(task_resource_profiles_, tasks_, resources_);
    validate_message_routes(message_routes_, tasks_);
}

} // namespace cpssim
