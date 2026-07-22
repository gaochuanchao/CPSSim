/***
 * File: src/cpssim/gui/presentation_model.cpp
 * Purpose: Build complete, validated, and deterministically ordered detached
 *          experiment presentation records.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/presentation_model.hpp"

#include "cpssim/gui/draft_run_plan.hpp"
#include "cpssim/gui/editable_system_draft.hpp"

#include <algorithm>
#include <stdexcept>

namespace cpssim {
namespace {

/*** Reports whether a task may execute on the selected configured resource. ***/
bool has_profile(const ExperimentConfig& config, TaskId task_id, ResourceId resource_id) {
    return std::any_of(config.task_resource_profiles().begin(),
                       config.task_resource_profiles().end(),
                       [task_id, resource_id](const TaskResourceProfile& profile) {
                           return profile.task_id == task_id && profile.resource_id == resource_id;
                       });
}

/*** Reports whether a stable resource identifier exists in the configuration. ***/
bool has_resource(const ExperimentConfig& config, ResourceId resource_id) {
    return std::any_of(
        config.resources().begin(), config.resources().end(),
        [resource_id](const ResourceSpec& resource) { return resource.id() == resource_id; });
}

/*** Validates and sorts the applied plan without changing allocation semantics. ***/
std::vector<GuiTaskAssignmentPresentation>
build_assignments(const ExperimentConfig& config, const std::vector<TaskAssignment>& assignments) {
    if (assignments.size() != config.tasks().size()) {
        throw std::invalid_argument{"GUI presentation requires one assignment per task"};
    }

    std::vector<GuiTaskAssignmentPresentation> result;
    result.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        if (!has_resource(config, assignment.resource_id) ||
            !has_profile(config, assignment.task_id, assignment.resource_id)) {
            throw std::invalid_argument{"GUI presentation received an invalid assignment"};
        }
        result.push_back({.task_id = assignment.task_id, .resource_id = assignment.resource_id});
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.task_id < right.task_id; });

    for (std::size_t index = 0; index < result.size(); ++index) {
        if (index > 0 && result[index - 1].task_id == result[index].task_id) {
            throw std::invalid_argument{"GUI presentation received duplicate task assignments"};
        }
    }

    std::vector<TaskId> configured_task_ids;
    configured_task_ids.reserve(config.tasks().size());
    for (const auto& task : config.tasks()) {
        configured_task_ids.push_back(task.id());
    }
    std::sort(configured_task_ids.begin(), configured_task_ids.end());
    for (std::size_t index = 0; index < result.size(); ++index) {
        if (result[index].task_id != configured_task_ids[index]) {
            throw std::invalid_argument{"GUI presentation assignment references an unknown task"};
        }
    }

    return result;
}

} // namespace

/*** Copies validated configuration into sorted presentation values. ***/
ExperimentPresentationSnapshot build_experiment_presentation(const ExperimentConfig& config) {
    ExperimentPresentationSnapshot result;
    result.tick_period = config.tick_period();
    result.preemption_mode = config.scheduling().preemption_mode;

    result.resources.reserve(config.resources().size());
    for (const auto& resource : config.resources()) {
        result.resources.push_back({.id = resource.id(), .name = resource.name()});
    }
    std::sort(result.resources.begin(), result.resources.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });

    result.tasks.reserve(config.tasks().size());
    for (const auto& task : config.tasks()) {
        result.tasks.push_back({.id = task.id(),
                                .name = task.name(),
                                .period = task.period(),
                                .deadline = task.deadline(),
                                .offset = task.offset(),
                                .priority = task.priority()});
    }
    std::sort(result.tasks.begin(), result.tasks.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });

    result.profiles.reserve(config.task_resource_profiles().size());
    for (const auto& profile : config.task_resource_profiles()) {
        result.profiles.push_back({.task_id = profile.task_id,
                                   .resource_id = profile.resource_id,
                                   .execution_time = profile.execution_time});
    }
    std::sort(result.profiles.begin(), result.profiles.end(),
              [](const auto& left, const auto& right) {
                  if (left.task_id != right.task_id) {
                      return left.task_id < right.task_id;
                  }
                  return left.resource_id < right.resource_id;
              });

    result.routes.reserve(config.message_routes().size());
    for (const auto& route : config.message_routes()) {
        result.routes.push_back({.identity = {.source_task_id = route.source_task_id,
                                              .destination_task_id = route.destination_task_id},
                                 .send_offset = route.send_offset,
                                 .delay = route.delay});
    }
    std::sort(result.routes.begin(), result.routes.end(),
              [](const auto& left, const auto& right) { return left.identity < right.identity; });

    return result;
}

/*** Adds one validated applied placement to detached configuration values. ***/
ExperimentPresentationSnapshot
build_experiment_presentation(const ExperimentConfig& config,
                              const std::vector<TaskAssignment>& assignments) {
    auto result = build_experiment_presentation(config);
    result.assignments = build_assignments(config, assignments);
    return result;
}

ExperimentPresentationSnapshot
build_draft_experiment_presentation(const EditableSystemDraft& draft,
                                    const std::vector<DraftTaskAssignment>& assignments) {
    ExperimentPresentationSnapshot result;
    result.tick_period = std::chrono::nanoseconds{draft.tick_period_ns()};
    result.preemption_mode = draft.preemption_mode();
    for (const auto& resource : draft.resources()) {
        result.resources.push_back({resource.id, resource.name});
    }
    for (const auto& task : draft.tasks()) {
        result.tasks.push_back(
            {task.id, task.name, task.period, task.deadline, task.offset, task.priority});
    }
    for (const auto& profile : draft.profiles()) {
        result.profiles.push_back({profile.task_id, profile.resource_id, profile.execution_time});
    }
    for (const auto& route : draft.routes()) {
        result.routes.push_back(
            {{route.source_task_id, route.destination_task_id}, route.send_offset, route.delay});
    }
    for (const auto& assignment : assignments) {
        if (!assignment.resource_id.has_value()) {
            continue;
        }
        const auto task =
            std::find_if(result.tasks.begin(), result.tasks.end(),
                         [&](const auto& row) { return row.id == assignment.task_id; });
        const auto resource =
            std::find_if(result.resources.begin(), result.resources.end(),
                         [&](const auto& row) { return row.id == *assignment.resource_id; });
        if (task != result.tasks.end() && resource != result.resources.end()) {
            result.assignments.push_back({assignment.task_id, *assignment.resource_id});
        }
    }
    return result;
}

/*** Finds a resource in the presentation's canonical ID order. ***/
const GuiResourcePresentation* find_resource(const ExperimentPresentationSnapshot& experiment,
                                             ResourceId resource_id) {
    const auto found = std::lower_bound(
        experiment.resources.begin(), experiment.resources.end(), resource_id,
        [](const GuiResourcePresentation& resource, ResourceId id) { return resource.id < id; });
    return found != experiment.resources.end() && found->id == resource_id ? &*found : nullptr;
}

/*** Finds a task in the presentation's canonical ID order. ***/
const GuiTaskPresentation* find_task(const ExperimentPresentationSnapshot& experiment,
                                     TaskId task_id) {
    const auto found =
        std::lower_bound(experiment.tasks.begin(), experiment.tasks.end(), task_id,
                         [](const GuiTaskPresentation& task, TaskId id) { return task.id < id; });
    return found != experiment.tasks.end() && found->id == task_id ? &*found : nullptr;
}

/*** Finds the one applied assignment for a configured task. ***/
const GuiTaskAssignmentPresentation*
find_assignment(const ExperimentPresentationSnapshot& experiment, TaskId task_id) {
    const auto found =
        std::lower_bound(experiment.assignments.begin(), experiment.assignments.end(), task_id,
                         [](const GuiTaskAssignmentPresentation& assignment, TaskId id) {
                             return assignment.task_id < id;
                         });
    return found != experiment.assignments.end() && found->task_id == task_id ? &*found : nullptr;
}

/*** Finds a configured route by its stable unique endpoint pair. ***/
const GuiMessageRoutePresentation* find_route(const ExperimentPresentationSnapshot& experiment,
                                              GuiRouteIdentity route_id) {
    const auto found =
        std::lower_bound(experiment.routes.begin(), experiment.routes.end(), route_id,
                         [](const GuiMessageRoutePresentation& route, GuiRouteIdentity id) {
                             return route.identity < id;
                         });
    return found != experiment.routes.end() && found->identity == route_id ? &*found : nullptr;
}

} // namespace cpssim
