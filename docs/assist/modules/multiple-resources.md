# Module: Multiple Independent Resources

## Purpose

Route fixed-assignment jobs to independent exclusive resources while
preserving one deterministic global event timeline.

## Public interfaces

- `ConfiguredResourceAllocator`: retain an explicit task/resource plan.
- `Scheduler::resource_count`: report configured runtime resources.
- `Scheduler::resource(ResourceId)`: expose one resource read-only.
- `Scheduler::ready_jobs(ResourceId)`: expose Ready identities read-only.
- `Resource::busy_ticks_until` and `idle_ticks_until`: expose exact integer
  accounting over `[0, observation_tick)`.

## Owned state

Runtime `Scheduler` owns state sorted by `ResourceId`. Each entry contains one
runtime `Resource` and that resource's Ready identities. Each `Resource` owns
its optional Running identity, active interval, expected completion, and busy
ticks.

The engine owns one global `EventQueue`, release model, current tick, and
append-only trace. This preserves deterministic phase processing across all
resources while leaving job/resource mechanism with the scheduler.

## Invariants

- Every configured task has exactly one accessible resource assignment.
- Every released job remains on its captured resource.
- A job appears in only that resource's Ready or Running membership.
- Each resource has at most one Running job.
- Execution charging and stale-completion matching use that resource's own
  markers.
- All pre-scheduling phases finish before runtime scheduling at a tick.
- Resource scheduling occurs in ascending `ResourceId`.
- No resource can preempt, charge, or complete another resource's job.

## Failure behavior

Engine construction rejects incomplete, duplicate, unknown, or inaccessible
assignment plans. Scheduler resource lookup rejects an unknown `ResourceId`.
Resource/job mismatch and execution-marker inconsistencies throw
`std::logic_error` rather than producing a misleading trace.

## Dependencies

```text
ExperimentConfig -> ResourceAllocator -> TaskAssignment plan
PeriodicReleaseModel + SchedulingPolicy + Resource + EventQueue
    -> Scheduler -> SimulationEngine global orchestration
```

The module has no network, MATLAB, Bosch, FMI, GUI, operating-system thread, or
wall-clock dependency.

## References

- [Module interactions](../MODULE-INTERACTIONS.md)
- [ADR-0008](../adr/0008-order-independent-resources-by-resource-id.md)
- [ADR-0009](../adr/0009-separate-scheduler-policy-resource-and-engine-ownership.md)
- [scheduler.hpp](../../src/cpssim/kernel/scheduler.hpp)
- [simulation_engine.hpp](../../src/cpssim/kernel/simulation_engine.hpp)
- [resource_allocator.hpp](../../src/cpssim/policy/resource_allocator.hpp)
- [scheduler tests](../../tests/kernel/scheduler_test.cpp)
- [multi-resource tests](../../tests/kernel/multi_resource_simulation_test.cpp)
