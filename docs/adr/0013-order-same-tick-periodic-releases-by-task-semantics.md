# ADR-0013: Order Same-Tick Periodic Releases by Task Semantics

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T7, T8, T9, T12

## Context

T12 found the first behavioral difference between CPSSim and both MATLAB
timing references at tick 100. Estimator's tick-100 successor was inserted
while its tick-0 release was processed. Sensor's tick-100 successor was
inserted later, while its tick-50 release was processed. The generic event
queue therefore returned Estimator before Sensor by insertion sequence.

MATLAB orders all jobs released at one tick by smaller numeric priority and
then smaller `TaskId`. This produces Sensor before Estimator. The event counts
and scheduling outcome were unchanged, but the processed release trace was
observably different.

Precomputing all releases would also reproduce MATLAB ordering, but it would
reverse the accepted T8 runtime-task design. Adding task priority to every
generic event or queue comparator would make the event queue depend on
periodic-task scheduling semantics.

## Decision

`SimulationEngine` processes each tick's `JobRelease` events as one batch.
After completion, delivery, and deadline phases have finished, it:

1. removes all pending `JobRelease` events at that tick from `EventQueue`;
2. orders the batch by smaller task priority, then smaller `TaskId`;
3. delegates each release to its runtime `Task` and `Scheduler`; and
4. invokes runtime job scheduling only after the complete batch is submitted.

The queue retains its generic `(tick, phase, EventSequence)` removal contract.
`EventSequence` remains the stable insertion identity assigned when the
release candidate was scheduled; it is not periodic-task precedence and is
not rewritten during batching.

Every runtime `Task` still owns its release position, creates a job only when
its release is processed, and schedules only one successor. The complete
release calendar is not precomputed.

This is a domain-level refinement of
[ADR-0004](0004-order-events-by-tick-phase-and-sequence.md), not a change to
the reusable event queue. It is also consistent with ADR-0004's statement that
insertion identity is not globally identical to processed-trace order.

## Consequences

Positive:

- CPSSim reproduces the MATLAB release-row order;
- dynamically generated successors behave like synchronous task releases;
- all same-tick jobs remain visible before the scheduler selects work;
- the generic queue gains no priority or task-configuration dependency; and
- one pending release per runtime task is preserved.

Limiting:

- processed release order can differ from `EventSequence` order within one
  tick and phase;
- code reading a canonical trace must treat sequence as event identity, not a
  line number;
- the engine must have access to runtime task specifications when ordering the
  release batch; and
- future nonperiodic activation sources need their own explicit batching rule
  rather than silently inheriting this periodic-task rule.

## Validation

- a focused engine test creates successors inserted at different earlier
  ticks and verifies priority/`TaskId` release ordering;
- dedicated and shared-cloud conformance tests must pass all normalized
  scheduler rows; and
- existing queue tests must continue to verify equal-phase insertion order
  when `EventQueue` is used directly.

