# ADR-0004: Order Events by Tick, Phase, and Insertion Sequence

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T6, T7, T8, T9, T11
- Confirmed: 2026-07-18 by T11 positive-offset fixed-delay message processing

## Context

CPSSim must return pending events in the same order on every supported build.
Several events may have the same integer tick, and standard container behavior
or enum numeric values must not decide their observable order accidentally.

T6 introduced `Tick`, `EventPhase`, and `EventSequence` but deliberately left
ordering and sequence allocation to T7. During T7 review, superdense time
`(tick, microstep)` was considered because FMI uses it to describe event
iterations. The target Bosch model is an FMI 2.0 Co-Simulation FMU, whose
`fmi2DoStep` interface receives a real communication point and a positive step
size rather than a microstep. Its internal event iterations are not controlled
by CPSSim's event queue. The
[official FMI 2.0 specification](https://fmi-standard.org/assets/releases/FMI_for_ModelExchange_and_CoSimulation_v2.0.pdf)
also describes superdense time as grouping values belonging to one model
evaluation, which is not the current role of one CPSSim trace record.

Renaming the global `EventSequence` to `Microstep` would conflate two concepts:
a sequence is a stable insertion identity, while a genuine microstep is part
of logical time and normally restarts or changes meaning with physical time.

## Decision

`EventQueue` owns pending `Event` values and one zero-based sequence allocator.
Every successful `schedule` call receives the next `EventSequence`. A rejected
event is validated before queue state changes and does not consume a sequence.
The maximum `std::uint64_t` value can be assigned once; later insertion throws
`std::overflow_error`.

The queue compares events lexicographically using:

1. smaller `Tick` first;
2. at equal tick, smaller explicit phase precedence first; and
3. at equal tick and phase, smaller `EventSequence` first.

The accepted phase precedence is:

1. `ExecutionCompletion`;
2. `MessageDelivery`;
3. `DeadlineCheck`;
4. `JobRelease`;
5. `PolicyUpdate`;
6. `Scheduling`; and
7. `CausedAction`.

This is one same-tick processing cycle. Completion first ensures that a job
finishing exactly at its deadline is settled before deadline checking. Message
delivery then makes newly arrived data visible. Deadline checking observes
unfinished existing jobs before new releases are introduced. Policy updates
and scheduling see all preceding facts, and caused actions follow the
decisions that produce them.

T7 validates the deterministic queue rule, while later event-producing and
MATLAB-conformance tasks validate the domain semantics. A reference mismatch
requires a documented ordering revision; it must not be hidden by changing
tests alone.

The comparator maps every phase explicitly. Enum declaration order and
underlying numeric values are not part of the ordering contract. Although C++
currently assigns implicit integers to `EventPhase`, using those values would
allow declaration reordering to change behavior silently. The explicit switch
keeps semantic names separate from kernel policy and rejects invalid values.

`schedule` receives the producer-known event fields rather than a completed
`Event`, because a completed record already contains the sequence that the
queue must allocate. A separate request/draft record was considered but
deferred: the current five-argument interface is small, and another type would
not enforce an additional invariant.

`EventSequence` is a unique insertion identity and stable final tie-breaker.
It is not a timestamp and not a promise that events are removed in increasing
sequence globally. For example, a later-inserted event at tick 5 is removed
before an earlier-inserted event at tick 100.

Superdense time is deferred. If a later FMI event-mode, fixed-point, or other
co-simulation requirement needs model-evaluation rounds at the same tick, it
must introduce a distinct `Microstep` through a new ADR. That change must also
address event JSON schema compatibility and whether multiple records may share
one superdense instant.

## Consequences

Positive:

- pending-event order is deterministic and directly testable;
- phase semantics are independent of enum representation;
- equal-tick/equal-phase events retain insertion order;
- causal references can use the sequence returned by `schedule`;
- invalid insertion cannot create sequence gaps; and
- the core remains independent of FMI and floating-point time.

Negative or limiting:

- phase precedence is fixed kernel semantics and changing it requires an
  explicit decision plus test updates;
- insertion sequence and removal order are different concepts;
- arbitrary event cancellation is not implemented;
- the queue does not prevent a caller from scheduling an event earlier than a
  simulation engine's current tick because T7 introduces no engine clock; and
- genuine superdense event-iteration semantics remain deferred.

## Validation

- tests insert equal-phase events and verify stable sequence order;
- tests insert phases in reverse and verify the complete explicit precedence;
- tests verify that tick takes priority over phase;
- tests verify causal-sequence retention; and
- tests verify empty-queue errors and transactional sequence allocation.

## Follow-up

- T8 may use `EventQueue::schedule` to create periodic releases.
- T9's `SimulationEngine` now rejects backward event time and implements the
  completion/deadline/release/scheduling cycle. Later caused-action producers
  may extend same-tick fixed-point behavior.
- Cancellation requires a separate strategy once a real producer needs it.
- FMI adapters batch or translate events without placing FMI types in this
  queue.
