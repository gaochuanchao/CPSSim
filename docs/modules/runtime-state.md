# Module: Runtime State

## Purpose

Represent a released job and an exclusive execution resource while preserving
lifecycle, resource identity, and integer accounting invariants.

## Ownership

- `JobState` owns stable identity, captured resource/timing/priority, remaining
  execution, lifecycle, first start, finish, preemption count, and deadline
  outcome.
- `Resource` owns a copied `ResourceSpec`, optional Running identity, active
  interval, expected completion, and integer busy-time accounting.
- Runtime `Scheduler` owns the canonical job collection, one Ready-identity
  vector per resource, and all runtime resources.
- `SimulationEngine` owns global time, event routing, release progression, and
  the append-only trace.

`JobId` is task-local; collections use `(TaskId, JobId)` through
`JobIdentity`.

## Transitions

- `Scheduler::submit`: own a new Ready job and register its Ready identity.
- `Resource::start_job`: start a scheduler-selected Ready job.
- `Resource::preempt_job`: charge and return the active job to Ready lifecycle.
- `Resource::charge_execution`: charge its active interval and complete at zero.
- `JobState::mark_deadline_missed`: record one incomplete-at-deadline outcome.

Only `Resource` can mutate private job execution/lifecycle fields. `Scheduler`
coordinates Ready membership around those transitions. A resource mismatch,
duplicate membership, invalid interval, or illegal lifecycle transition
throws.

## Assignment relationship

The job's resource and execution demand are captured by runtime
`Task::release`; they do not come from `TaskSpec`. A released job does not
migrate when its producing task is later reassigned.

## Boundary

The model is exclusive: one Running job per resource. The runtime scheduler
owns the job store and Ready membership, while each resource owns independent
execution markers and accounting. `ResourceAllocator` and `SchedulingPolicy`
remain separate read-only decision interfaces. Fractional or spatial
allocation is deferred until its semantics are designed.

## References

- [runtime_state.hpp](../../src/cpssim/model/runtime_state.hpp)
- [runtime_state_test.cpp](../../tests/model/runtime_state_test.cpp)
- [Simulation semantics](../guide/SIMULATION-SEMANTICS.md)
- [ADR-0006](../adr/0006-assign-resources-to-runtime-tasks.md)
- [ADR-0009](../adr/0009-separate-scheduler-policy-resource-and-engine-ownership.md)
- [Scheduling module](fixed-priority-scheduling.md)
- [Multiple-resource module](multiple-resources.md)
