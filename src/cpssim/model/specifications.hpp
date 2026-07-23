/***
 * File: src/cpssim/model/specifications.hpp
 * Purpose: Declare validated immutable resource and periodic-task
 *          specifications used to configure CPSSim experiments.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: These values describe configuration, not changing runtime state.
 *        Canonical timing fields use integer Tick values.
 ***/

#pragma once

#include "cpssim/model/categories.hpp"
#include "cpssim/model/identifiers.hpp"
#include "cpssim/model/time.hpp"

#include <cstdint>
#include <string>

namespace cpssim {

using Priority = std::int32_t;

/*** Groups the periodic release timing of a task in integer ticks. ***/
struct PeriodicTimingSpec {
    Tick period;
    Tick deadline;
    Tick offset;
};

/***
 * Describes one resource a task may use and its execution demand there.
 * The complete ExperimentConfig validates the referenced IDs and timing.
 ***/
struct TaskResourceProfile {
    TaskId task_id;
    ResourceId resource_id;
    Tick execution_time;
};

/*** Stores experiment-wide runtime scheduling behavior. ***/
struct SchedulingSpec {
    PreemptionMode preemption_mode;
};

/*** Fixed causal offset between source-job completion and MessageSend event.
 *  This is a core timing invariant, not a user-configurable channel parameter.
 *  source completion at tick t
 *  MessageSend at t + message_route_send_offset_ticks
 *  MessageDelivery at t + message_route_send_offset_ticks + configured delay
 ***/
inline constexpr Tick message_route_send_offset_ticks = Tick{1};

/*** Defines one completion-triggered task route with fixed integer timing. ***/
struct MessageRouteSpec {
    TaskId source_task_id;
    TaskId destination_task_id;
    Tick send_offset;
    Tick delay;
};

/*** Stores the immutable description of one execution or communication resource. ***/
class ResourceSpec {
  public:
    /***
     * Creates a resource specification with a stable ID and nonempty name.
     * Throws std::invalid_argument when name is empty.
     ***/
    ResourceSpec(ResourceId id, std::string name);

    // Returns the resource's stable identifier.
    ResourceId id() const { return id_; }

    // Returns a read-only reference to the resource name.
    const std::string& name() const { return name_; }

  private:
    ResourceId id_;
    std::string name_;
};

/*** Stores one validated immutable periodic-task configuration. ***/
class TaskSpec {
  public:
    /***
     * Creates and validates an immutable periodic-task specification.
     * Throws std::invalid_argument for an empty name, invalid timing, or a
     * negative priority.
     ***/
    TaskSpec(TaskId id, std::string name, PeriodicTimingSpec timing, Priority priority);

    // Returns the task's stable identifier.
    TaskId id() const { return id_; }

    // Returns a read-only reference to the task name.
    const std::string& name() const { return name_; }

    // Returns the task's periodic release interval in ticks.
    Tick period() const { return timing_.period; }

    // Returns the task's relative deadline in ticks.
    Tick deadline() const { return timing_.deadline; }

    // Returns the first-release offset in ticks.
    Tick offset() const { return timing_.offset; }

    // Returns the configured scheduling priority value.
    Priority priority() const { return priority_; }

  private:
    TaskId id_;
    std::string name_;
    PeriodicTimingSpec timing_;
    Priority priority_;
};

} // namespace cpssim
