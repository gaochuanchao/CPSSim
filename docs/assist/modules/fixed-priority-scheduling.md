# Module: Fixed-Priority Scheduling

## Purpose

Run the event-driven fixed-priority baseline while keeping initial task
placement, read-only job ordering, runtime scheduling mechanism, resource
execution, and global orchestration separate.

## Public interfaces

- `ResourceAllocator::allocate`: return a complete task-placement plan.
- `SingleResourceAllocator`: assign each task to the only configured resource.
- `SchedulingPolicy`: common read-only runtime job-selection interface.
- `FixedPriorityPolicy::select`: choose the best Ready `JobIdentity` by
  priority, release tick, task ID, and job ID.
- `FixedPriorityPolicy::should_preempt`: return true only for strictly higher
  Ready priority.
- `Scheduler`: own jobs, Ready queues, resources, and scheduling mechanism.
- `SimulationEngine`: own global time, events, releases, and processed trace.

## Owned state

The engine owns the queue, release model, current tick, and append-only trace.
Runtime `Scheduler` owns jobs, per-resource Ready membership, and resources.
Each `Resource` owns its Running interval, expected completion, and busy-time
accounting. Allocators and scheduling policies own no mutable simulation
state. Runtime `Task` owns its applied assignment and release position.

## Invariants

- Canonical time never moves backward.
- An allocation plan contains exactly one valid assignment per task.
- Every in-horizon task has one resource assignment before release.
- A Running job is charged once for each elapsed interval.
- Expected completion matches both Running identity and tick.
- Completion reaches exactly zero remaining execution.
- Completion at a deadline precedes the deadline check.
- All same-tick releases precede runtime scheduling.
- Equal priority does not preempt.
- Non-preemptive mode never replaces a Running job.
- One task cannot have two active jobs in the current baseline.
- Only successful state observations enter the processed trace.

## Failure behavior

Construction rejects incomplete, duplicate, unknown, or inaccessible
assignment plans. `run` rejects repeated execution and self-overlap. Policy
methods reject empty or inconsistent views. Scheduler or engine state/queue
disagreement throws `std::logic_error` rather than producing a misleading
trace.

## Dependencies

```text
ExperimentConfig <- ResourceAllocator
model records <- SchedulingPolicy <- FixedPriorityPolicy
model records + EventQueue + SchedulingPolicy <- Scheduler
EventQueue + PeriodicReleaseModel + allocator plan + Scheduler
    <- SimulationEngine
```

The module has no MATLAB, Bosch, FMI, GUI, network, or wall-clock dependency.

## References

- [Simulation semantics](../guide/SIMULATION-SEMANTICS.md)
- [ADR-0007](../adr/0007-separate-fixed-priority-policy-from-event-driven-mechanism.md)
- [ADR-0009](../adr/0009-separate-scheduler-policy-resource-and-engine-ownership.md)
- [simulation_engine.hpp](../../src/cpssim/kernel/simulation_engine.hpp)
- [resource_allocator.hpp](../../src/cpssim/policy/resource_allocator.hpp)
- [scheduling_policy.hpp](../../src/cpssim/policy/scheduling_policy.hpp)
- [scheduler.hpp](../../src/cpssim/kernel/scheduler.hpp)
- [fixed_priority.hpp](../../src/cpssim/policy/fixed_priority.hpp)
- [engine tests](../../tests/kernel/simulation_engine_test.cpp)
- [scheduler tests](../../tests/kernel/scheduler_test.cpp)
- [allocator tests](../../tests/policy/resource_allocator_test.cpp)
- [policy tests](../../tests/policy/fixed_priority_test.cpp)
