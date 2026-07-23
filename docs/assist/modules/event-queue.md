# Module: Event Queue

## Responsibility

Own pending canonical events, allocate stable insertion sequences, and expose
the next event according to deterministic tick, phase, and sequence ordering.

## Public interface

[event_queue.hpp](../../src/cpssim/kernel/event_queue.hpp) provides
`EventQueue` with:

- `schedule` to validate, assign a sequence, and insert an event;
- `next` to inspect the selected event;
- `pop_next` to remove and return it; and
- `empty` and `size` for collection state.

The learning-oriented explanation is in
[Simulation semantics](../guide/SIMULATION-SEMANTICS.md#same-tick-order), and
the durable ordering contract is
[ADR-0004](../adr/0004-order-events-by-tick-phase-and-sequence.md).

`schedule` accepts the producer-known fields instead of a completed `Event`
because the queue, not the caller, owns the missing sequence assignment. A
separate request type is intentionally deferred until it protects a concrete
need rather than only renaming this short parameter list.

## Owned state

Each `EventQueue` owns:

- its pending `Event` values;
- the next zero-based insertion-sequence value; and
- whether the sequence domain has been exhausted.

It does not own runtime job/resource state, a current simulation tick,
scheduling policy, event-processing behavior, or canonical trace history.
The [periodic-release module](periodic-releases.md) inserts first and
successor releases through this public interface; the engine-owned queue keeps
ownership of all pending events.

The [causal-message module](causal-messages.md) inserts caused sends and
deliveries through the same interface. Positive route timing avoids inserting
a new earlier-phase event at a tick whose final phase is already processing.

The [fixed-priority scheduling module](fixed-priority-scheduling.md)
supplies the current-tick guard and processed history. It handles preempted
completion candidates through expected-tick validation rather than adding
general queue cancellation.

## Invariants

- Smaller tick is selected first.
- At equal tick, explicit phase precedence is selected first.
- At equal tick and phase, smaller insertion sequence is selected first.
- Successful insertions receive unique increasing sequence values.
- Rejected insertions do not consume a sequence.
- Enum numeric values do not determine phase precedence.
- Pending events are owned values rather than external mutable references.

## Dependencies

The module depends on the canonical event model and C++20 standard-library
containers. Network code depends on this queue, not the reverse. The queue has
no JSON, runtime-state, policy, Bosch, FMI, MATLAB, Simulink, or GUI dependency.

## Failure behavior

- Invalid event fields propagate `std::invalid_argument` from `Event`.
- Empty `next` and `pop_next` calls throw `std::out_of_range`.
- Sequence exhaustion throws `std::overflow_error`.
- An enum value outside the phase vocabulary throws `std::logic_error` when
  the comparator must rank it.
- Failed scheduling leaves the queue and allocator unchanged.

## Example

```cpp
EventQueue queue;
queue.schedule(20, EventPhase::Scheduling, EventType::JobStart);
queue.schedule(10, EventPhase::JobRelease, EventType::JobRelease);

const Event first = queue.pop_next(); // tick 10
```

The complete ordering and failure contract is verified by
[event_queue_test.cpp](../../tests/kernel/event_queue_test.cpp).
