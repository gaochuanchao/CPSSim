/***
 * File: src/cpssim/gui/presentation_model.hpp
 * Purpose: Declare detached, deterministically ordered experiment records for
 *          read-only GUI presentation.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: These value records copy validated configuration and applied
 *        assignment data; they expose no mutable simulator containers.
 ***/

#pragma once

#include "cpssim/model/experiment_config.hpp"
#include "cpssim/policy/resource_allocator.hpp"

#include <compare>
#include <string>
#include <vector>

namespace cpssim {

/*** Copies one immutable resource identity and display name. ***/
struct GuiResourcePresentation {
    ResourceId id;
    std::string name;

    bool operator==(const GuiResourcePresentation&) const = default;
};

/*** Copies one immutable periodic task definition. ***/
struct GuiTaskPresentation {
    TaskId id;
    std::string name;
    Tick period;
    Tick deadline;
    Tick offset;
    Priority priority;

    bool operator==(const GuiTaskPresentation&) const = default;
};

/*** Copies one task-resource execution profile. ***/
struct GuiTaskResourceProfilePresentation {
    TaskId task_id;
    ResourceId resource_id;
    Tick execution_time;

    bool operator==(const GuiTaskResourceProfilePresentation&) const = default;
};

/*** Provides stable identity for a configured route's unique endpoint pair. ***/
struct GuiRouteIdentity {
    TaskId source_task_id;
    TaskId destination_task_id;

    auto operator<=>(const GuiRouteIdentity&) const = default;
};

/*** Copies one completion-triggered fixed-delay message route. ***/
struct GuiMessageRoutePresentation {
    GuiRouteIdentity identity;
    Tick send_offset;
    Tick delay;

    bool operator==(const GuiMessageRoutePresentation&) const = default;
};

/*** Copies one applied task-to-resource assignment. ***/
struct GuiTaskAssignmentPresentation {
    TaskId task_id;
    ResourceId resource_id;

    bool operator==(const GuiTaskAssignmentPresentation&) const = default;
};

/*** Owns the complete detached read-only description used by G02 views. ***/
struct ExperimentPresentationSnapshot {
    PhysicalDuration tick_period{};
    PreemptionMode preemption_mode{PreemptionMode::Preemptive};
    std::vector<GuiResourcePresentation> resources;
    std::vector<GuiTaskPresentation> tasks;
    std::vector<GuiTaskResourceProfilePresentation> profiles;
    std::vector<GuiMessageRoutePresentation> routes;
    std::vector<GuiTaskAssignmentPresentation> assignments;

    bool operator==(const ExperimentPresentationSnapshot&) const = default;
};

/*** Copies configuration presentation before any run plan is active. ***/
ExperimentPresentationSnapshot build_experiment_presentation(const ExperimentConfig& config);

/***
 * Copies and deterministically sorts validated experiment and assignment data.
 * Throws std::invalid_argument if assignments are incomplete, duplicated,
 * unknown, or inaccessible.
 ***/
ExperimentPresentationSnapshot
build_experiment_presentation(const ExperimentConfig& config,
                              const std::vector<TaskAssignment>& assignments);

// Finds one immutable presentation record by stable identifier.
const GuiResourcePresentation* find_resource(const ExperimentPresentationSnapshot& experiment,
                                             ResourceId resource_id);
const GuiTaskPresentation* find_task(const ExperimentPresentationSnapshot& experiment,
                                     TaskId task_id);
const GuiTaskAssignmentPresentation*
find_assignment(const ExperimentPresentationSnapshot& experiment, TaskId task_id);
const GuiMessageRoutePresentation* find_route(const ExperimentPresentationSnapshot& experiment,
                                              GuiRouteIdentity route_id);

} // namespace cpssim
