/***
 * File: src/cpssim/kernel/simulation_engine.hpp
 * Purpose: Declare resumable global deterministic time, event routing,
 *          release and functional orchestration, and append-only trace
 *          ownership.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Runtime Scheduler owns jobs, Ready queues, and resources. The engine
 *        orchestrates it but does not implement scheduling decisions.
 ***/

#pragma once

#include "cpssim/functional/functional_runtime.hpp"
#include "cpssim/kernel/periodic_release.hpp"
#include "cpssim/kernel/scheduler.hpp"
#include "cpssim/network/fixed_delay_network.hpp"
#include "cpssim/policy/resource_allocator.hpp"
#include "cpssim/policy/scheduling_policy.hpp"

#include <optional>
#include <vector>

namespace cpssim {

/*** Runs one deterministic experiment over one runtime scheduling domain. ***/
class SimulationEngine {
  public:
    /***
     * Builds runtime tasks and one Scheduler from immutable configuration. The
     * allocator must return one accessible assignment per task, and policy
     * must outlive this engine. Throws when allocation invariants are broken.
     ***/
    SimulationEngine(const ExperimentConfig& config, Tick stop_tick,
                     const ResourceAllocator& resource_allocator,
                     SchedulingPolicy& scheduling_policy);

    /***
     * Adds one non-owned functional model to the normal engine. The model and
     * policy must outlive this engine. Observations precede same-tick policy
     * decisions; accepted events become the action batch for the next interval.
     ***/
    SimulationEngine(const ExperimentConfig& config, Tick stop_tick,
                     const ResourceAllocator& resource_allocator,
                     SchedulingPolicy& scheduling_policy, FunctionalModel& functional_model);

    /***
     * Processes the experiment through its inclusive stop tick exactly once.
     * Throws for repeated runs or an internal event/state inconsistency.
     ***/
    void run();

    /***
     * Processes every event phase at the next pending logical tick. The first
     * call initializes releases and an optional functional model. Returns true
     * when one tick was processed, or false after the inclusive horizon is
     * complete. Calls after completion are safe and return false.
     ***/
    bool step_to_next_event();

    // Reports whether no further in-horizon event tick remains to process.
    bool finished() const { return finished_; }

    // Returns the last logical tick processed by this run.
    Tick current_tick() const { return current_tick_; }

    // Returns the runtime scheduling domain as a read-only view.
    const Scheduler& scheduler() const { return scheduler_; }

    // Returns the experiment scheduling assumptions known by the engine.
    const SchedulingSpec& scheduling() const { return scheduler_.scheduling(); }

    // Returns completion-triggered messages and network state read-only.
    const FixedDelayNetwork& network() const { return network_; }

    // Returns successfully processed canonical events in append-only order.
    const std::vector<Event>& trace() const { return trace_; }

    /***
     * Returns the online typed functional trace. It is empty when this engine
     * was constructed without a functional model.
     ***/
    const std::vector<FunctionalObservation>& functional_trace() const;

  private:
    // Performs the one-time release and optional functional initialization.
    void initialize();

    // Completes optional functional progression and marks the run finished.
    void finalize();

    // Processes every deterministic phase at one already-selected event tick.
    void process_event_tick(Tick tick);

    // Validates and applies one complete task-to-resource assignment plan.
    void apply_assignments(const ExperimentConfig& config,
                           const std::vector<TaskAssignment>& assignments);

    // Routes one completion, delivery, deadline, or release before scheduling.
    void process_pre_scheduling_event(const Event& event);

    // Processes all releases at one tick by task priority and then TaskId.
    void process_release_batch(Tick tick);

    // Processes Scheduling observations and CausedAction sends at this tick.
    void process_post_scheduling_events(Tick tick);

    // Forwards newly validated model rows to policy state in tick order.
    void forward_functional_observations(const std::vector<FunctionalObservation>& observations);

    EventQueue queue_;
    PeriodicReleaseModel releases_;
    Scheduler scheduler_;
    FixedDelayNetwork network_;
    std::vector<Event> trace_;
    std::optional<FunctionalRuntime> functional_runtime_;
    std::vector<FunctionalObservation> empty_functional_trace_;
    Tick stop_tick_;
    Tick current_tick_{0};
    bool initialized_{false};
    bool finished_{false};
    bool run_called_{false};
};

} // namespace cpssim
