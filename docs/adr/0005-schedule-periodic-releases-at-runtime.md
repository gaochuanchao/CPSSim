# ADR-0005: Schedule Periodic Releases at Runtime

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T5, T7, T8, T9, T12

## Context

An early T8 implementation generated the complete periodic calendar before
simulation, similar to MATLAB's offline job table. That reproduces known data
but does not represent a running task that releases one job, waits a period,
and releases the next. It also consumes memory proportional to the horizon and
makes later activation or cancellation awkward.

Pending events are not runtime history: an event becomes canonical history
only after the engine processes it successfully. MATLAB traces are validation
oracles, not the C++ simulator's internal architecture.

Job numbers are clearest as task-local sequences, but equal local numbers from
different tasks must remain distinct on shared resources.

## Decision

Each runtime `Task` owns its periodic release position. `PeriodicReleaseModel`
owns the task collection and coordinates deterministic initial insertion; it
does not maintain a separate per-task state record.

Initialization schedules only job 1 at each in-horizon task's offset. When the
engine pops a release, `Task::release`:

1. validates the event against the task's one pending release;
2. creates and returns the concrete Ready `JobState`; and
3. schedules only the next release at `current tick + period`, if it is within
   the inclusive stop tick.

Thus every task has at most one periodic release pending. The finite horizon is
a run constructor argument, not configuration. Negative horizons and integer
overflow are rejected.

Initial insertion order is smaller offset, smaller numeric priority, then
smaller `TaskId`. Thereafter T7 queue ordering determines event processing.

`JobId` is one-based and local to a task. Runtime collections use
`JobIdentity(TaskId, JobId)`; JSON retains separate readable fields.

Task resource assignment and per-resource execution are governed by
[ADR-0006](0006-assign-resources-to-runtime-tasks.md). Release generation
captures that assignment but does not select it.

## Consequences

- Long horizons do not require a precomputed calendar.
- Runtime `Task` has a clear reason to exist and a clear mutable owner.
- The queue represents pending work; a future logger represents processed
  append-only history.
- Task-local IDs remain readable without collisions.
- The engine must delegate every successfully popped periodic release exactly
  once.
- Inspecting all future releases requires running the deterministic progression.

## Validation

Tests cover one-pending-release behavior, offsets, inclusive horizon,
task-local IDs, invalid/duplicate progression, upper Tick boundaries, and
byte-identical release-only JSON output.

## Follow-up

T9 now stores returned jobs, registers them with their captured `Resource`,
runs policy decisions after same-tick releases, and appends successful
lifecycle events. T12 will compare produced history with MATLAB reference
traces.
