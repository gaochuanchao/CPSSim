/***
 * File: src/cpssim/kernel/simulation_engine.cpp
 * Purpose: Implement resumable global event and functional-model orchestration
 *          while delegating jobs, resources, execution, and dispatch.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Completion precedes deadline checking at the same tick. Scheduler
 *        applies configured preemption behavior and Resource transitions.
 ***/

#include "cpssim/kernel/simulation_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace cpssim {

namespace {

/*** Reports whether an event belongs to a phase before Scheduling. ***/
bool is_pre_scheduling(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
    case EventPhase::MessageDelivery:
    case EventPhase::DeadlineCheck:
    case EventPhase::JobRelease:
    case EventPhase::PolicyUpdate:
        return true;
    case EventPhase::Scheduling:
    case EventPhase::CausedAction:
        return false;
    }
    throw std::logic_error{"event phase is outside the scheduling cycle"};
}

} // namespace

/***
 * Creates the release and scheduling subsystems from the same immutable
 * configuration, then validates and applies the allocator's complete plan.
 ***/
SimulationEngine::SimulationEngine(const ExperimentConfig& config, Tick stop_tick,
                                   const ResourceAllocator& resource_allocator,
                                   SchedulingPolicy& scheduling_policy)
    : releases_{config, stop_tick},
      scheduler_{config.resources(), config.scheduling(), scheduling_policy, stop_tick},
      network_{config.message_routes(), stop_tick}, stop_tick_{stop_tick} {
    apply_assignments(config, resource_allocator.allocate(config));
}

/*** Adds a validated functional runtime without transferring model ownership. ***/
SimulationEngine::SimulationEngine(const ExperimentConfig& config, Tick stop_tick,
                                   const ResourceAllocator& resource_allocator,
                                   SchedulingPolicy& scheduling_policy,
                                   FunctionalModel& functional_model)
    : SimulationEngine{config, stop_tick, resource_allocator, scheduling_policy} {
    functional_runtime_.emplace(functional_model, config.tick_period(), stop_tick);
}

/*** Returns either the online runtime trace or a stable empty view. ***/
const std::vector<FunctionalObservation>& SimulationEngine::functional_trace() const {
    if (!functional_runtime_.has_value()) {
        return empty_functional_trace_;
    }
    return functional_runtime_->trace();
}

/*** Supplies consecutive physical observations to policy-owned state. ***/
void SimulationEngine::forward_functional_observations(
    const std::vector<FunctionalObservation>& observations) {
    for (const auto& observation : observations) {
        scheduler_.observe(observation);
    }
}

/***
 * Prevalidates plan size, uniqueness, task coverage, resource existence, and
 * accessibility before mutating any runtime Task assignment.
 ***/
void SimulationEngine::apply_assignments(const ExperimentConfig& config,
                                         const std::vector<TaskAssignment>& assignments) {
    if (assignments.size() != config.tasks().size()) {
        throw std::invalid_argument{"resource allocation must assign every task exactly once"};
    }

    for (const auto& task : config.tasks()) {
        std::optional<ResourceId> selected_resource;
        for (const auto& assignment : assignments) {
            if (assignment.task_id == task.id()) {
                if (selected_resource.has_value()) {
                    throw std::invalid_argument{
                        "resource allocation assigned a task more than once"};
                }
                selected_resource = assignment.resource_id;
            }
        }
        if (!selected_resource.has_value()) {
            throw std::invalid_argument{"resource allocation omitted a configured task"};
        }

        const auto resource_id = selected_resource.value();
        bool resource_exists = false;
        for (const auto& resource_spec : config.resources()) {
            if (resource_spec.id() == resource_id) {
                resource_exists = true;
                break;
            }
        }
        if (!resource_exists) {
            throw std::invalid_argument{"resource allocation selected an unknown resource"};
        }

        bool resource_is_accessible = false;
        for (const auto& profile : config.task_resource_profiles()) {
            if (profile.task_id == task.id() && profile.resource_id == resource_id) {
                resource_is_accessible = true;
                break;
            }
        }
        if (!resource_is_accessible) {
            throw std::invalid_argument{"resource allocation selected an inaccessible resource"};
        }
    }

    for (const auto& task : config.tasks()) {
        for (const auto& assignment : assignments) {
            if (assignment.task_id == task.id()) {
                releases_.assign_resource(assignment.task_id, assignment.resource_id);
                break;
            }
        }
    }
}

/***
 * Delegates completion/deadline state to Scheduler and converts a processed
 * Task release into scheduler-owned job submission. Only successful canonical
 * observations are appended to the trace.
 ***/
void SimulationEngine::process_pre_scheduling_event(const Event& event) {
    if (event.phase() == EventPhase::ExecutionCompletion && event.type() == EventType::JobFinish) {
        if (scheduler_.process_completion(event)) {
            trace_.push_back(event);
            network_.publish(event, queue_);
        }
        return;
    }

    if (event.phase() == EventPhase::MessageDelivery &&
        event.type() == EventType::MessageDelivery) {
        network_.process_delivery(event);
        trace_.push_back(event);
        return;
    }

    if (event.phase() == EventPhase::DeadlineCheck && event.type() == EventType::DeadlineMiss) {
        if (scheduler_.process_deadline(event)) {
            trace_.push_back(event);
        }
        return;
    }

    if (event.phase() == EventPhase::JobRelease && event.type() == EventType::JobRelease) {
        scheduler_.submit(releases_.release(event, queue_), queue_);
        trace_.push_back(event);
        return;
    }

    throw std::logic_error{"engine received an unsupported pre-scheduling event"};
}

/***
 * Removes every pending release at this tick, then processes the batch by
 * smaller numeric priority and smaller TaskId. The queue's insertion sequence
 * remains the event identity, but it does not encode periodic task precedence.
 ***/
void SimulationEngine::process_release_batch(Tick tick) {
    std::vector<Event> releases;
    while (!queue_.empty() && queue_.next().tick() == tick &&
           queue_.next().phase() == EventPhase::JobRelease) {
        releases.push_back(queue_.pop_next());
    }

    std::sort(releases.begin(), releases.end(), [this](const Event& left, const Event& right) {
        const auto left_task_id = left.entities().task_id;
        const auto right_task_id = right.entities().task_id;
        if (!left_task_id.has_value() || !right_task_id.has_value()) {
            throw std::logic_error{"periodic release batch lacks a task identity"};
        }
        const auto left_priority = releases_.task(*left_task_id).spec().priority();
        const auto right_priority = releases_.task(*right_task_id).spec().priority();
        if (left_priority != right_priority) {
            return left_priority < right_priority;
        }
        return *left_task_id < *right_task_id;
    });

    for (const auto& release : releases) {
        process_pre_scheduling_event(release);
    }
}

/***
 * Appends already-applied scheduling observations, then processes final-phase
 * sends that may schedule future deliveries. Positive delay prevents a new
 * same-tick pre-scheduling event from being inserted here.
 ***/
void SimulationEngine::process_post_scheduling_events(Tick tick) {
    while (!queue_.empty() && queue_.next().tick() == tick) {
        const auto event = queue_.pop_next();
        if (event.phase() == EventPhase::Scheduling) {
            trace_.push_back(event);
            continue;
        }
        if (event.phase() == EventPhase::CausedAction && event.type() == EventType::MessageSend) {
            network_.process_send(event, queue_);
            trace_.push_back(event);
            continue;
        }
        throw std::logic_error{"engine received an unsupported post-scheduling event"};
    }
}

/*** Initializes the pending release set and optional physical model once. ***/
void SimulationEngine::initialize() {
    if (initialized_) {
        return;
    }
    initialized_ = true;
    releases_.schedule_initial_releases(queue_);

    if (functional_runtime_.has_value()) {
        forward_functional_observations(functional_runtime_->initialize());
    }
}

/***
 * Advances an optional functional model through the inclusive horizon and
 * closes it exactly once. A headless engine retains its last event tick.
 ***/
void SimulationEngine::finalize() {
    if (finished_) {
        return;
    }

    if (functional_runtime_.has_value()) {
        forward_functional_observations(functional_runtime_->advance_to(stop_tick_));
        current_tick_ = stop_tick_;
        functional_runtime_->finalize();
    }
    finished_ = true;
}

/***
 * Completes pre-scheduling phases, invokes the runtime scheduling domain,
 * records caused actions, and forwards the accepted action batch.
 ***/
void SimulationEngine::process_event_tick(Tick tick) {
    if (tick < current_tick_) {
        throw std::logic_error{"simulation event time moved backward"};
    }
    if (functional_runtime_.has_value()) {
        forward_functional_observations(functional_runtime_->advance_to(tick));
    }
    current_tick_ = tick;
    const std::size_t action_begin = trace_.size();

    while (!queue_.empty() && queue_.next().tick() == tick &&
           is_pre_scheduling(queue_.next().phase()) &&
           queue_.next().phase() != EventPhase::JobRelease) {
        process_pre_scheduling_event(queue_.pop_next());
    }
    process_release_batch(tick);
    while (!queue_.empty() && queue_.next().tick() == tick &&
           is_pre_scheduling(queue_.next().phase())) {
        process_pre_scheduling_event(queue_.pop_next());
    }
    scheduler_.schedule(tick, queue_);
    process_post_scheduling_events(tick);

    if (functional_runtime_.has_value()) {
        const std::vector<Event> actions{trace_.begin() + static_cast<std::ptrdiff_t>(action_begin),
                                         trace_.end()};
        functional_runtime_->apply_actions(tick, actions);
    }
}

/*** Advances exactly one complete pending event tick without changing order. ***/
bool SimulationEngine::step_to_next_event() {
    if (finished_) {
        return false;
    }
    initialize();

    if (queue_.empty() || queue_.next().tick() > stop_tick_) {
        finalize();
        return false;
    }

    process_event_tick(queue_.next().tick());
    if (queue_.empty() || queue_.next().tick() > stop_tick_) {
        finalize();
    }
    return true;
}

/*** Processes all remaining event ticks through the same stepping path. ***/
void SimulationEngine::run() {
    if (run_called_) {
        throw std::logic_error{"simulation engine run can be called only once"};
    }
    run_called_ = true;
    while (step_to_next_event()) {
    }
}

} // namespace cpssim
