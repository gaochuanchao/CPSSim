# Module: Runtime Periodic Releases

## Purpose

Model runtime `Task` behavior: accept an external resource assignment, create
one concrete job when a release is processed, and schedule only that task's
successor.

## Ownership

Each `Task` owns its copied `TaskSpec`, accessible resource profiles, current
assignment, next tick/job number, and one captured pending resource.
`PeriodicReleaseModel` owns the task collection and coordinates deterministic
initial insertion and engine-facing lookup.

## Contract

- In-horizon tasks must be assigned an accessible resource before first
  release scheduling.
- At most one periodic release per task is pending.
- `Task::release` validates the exact pending event and returns a Ready
  `JobState` containing the captured resource and its profile execution time.
- Reassignment after scheduling does not change the pending job; it affects
  the successor.
- Task-local job IDs start at one and increment after processed releases.
- `stop_tick` is inclusive and all time is integer `Tick`.

## Determinism

Initial insertion uses `(offset, priority, TaskId)`. Queue ordering continues
to use `(tick, phase precedence, EventSequence)`. Release-only runs are tested
for byte-identical JSON output.

## Boundary

This module provides assignment/release mechanism but does not choose an
assignment or own a job store. The engine applies a `ResourceAllocator` plan
before composing it with runtime scheduling. The current engine supports several independent
resources, while migration and fractional allocation remain outside both
modules.

## References

- [periodic_release.hpp](../../src/cpssim/kernel/periodic_release.hpp)
- [periodic_release_test.cpp](../../tests/kernel/periodic_release_test.cpp)
- [Simulation semantics](../guide/SIMULATION-SEMANTICS.md#release-execution-and-completion-example)
- [ADR-0005](../adr/0005-schedule-periodic-releases-at-runtime.md)
- [ADR-0006](../adr/0006-assign-resources-to-runtime-tasks.md)
- [Scheduling module](fixed-priority-scheduling.md)
- [Multiple-resource module](multiple-resources.md)
