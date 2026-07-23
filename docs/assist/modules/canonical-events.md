# Module: Canonical Events

## Responsibility

Represent one portable event observation and serialize it as a deterministic,
versioned JSON line without implementing event generation, storage, or queue
ordering.

## Public interface

- [categories.hpp](../../src/cpssim/model/categories.hpp) provides
  `EventType` and `EventPhase`.
- [event.hpp](../../src/cpssim/model/event.hpp) provides `EventEntityRefs` and
  `Event`.
- [event_json.hpp](../../src/cpssim/trace/event_json.hpp) provides
  `serialize_event_json_line`.

Detailed field meanings and examples are in
[Simulation semantics](../guide/SIMULATION-SEMANTICS.md#trace-versus-queue).
The durable serialization contract is
[ADR-0003](../adr/0003-use-versioned-json-lines-event-records.md).
Queue ownership and ordering are documented separately in the
[event-queue module](event-queue.md) and
[ADR-0004](../adr/0004-order-events-by-tick-phase-and-sequence.md).

## Owned state

Each `Event` owns copies of its tick, phase, sequence, type, optional typed
entity IDs, and optional cause sequence. It exposes no field setters. The
module owns no global collection, file, queue, sequence counter, or mutable
trace.

## Invariants

- Tick is nonnegative canonical integer time.
- A present cause sequence is less than the event sequence.
- Entity domains use strong optional identifier types.
- Job-related records use `task_id` and task-local `job_id` together as their
  complete runtime identity.
- `MessageSend` identifies the source task/job and message; `MessageDelivery`
  identifies the destination task and message. Their causes form
  finish-to-send-to-delivery links.
- Phase names alone do not establish precedence; `EventQueue` uses an explicit
  mapping.
- Serialization uses schema version 1 and fixed field order/spellings.
- Absent optional values serialize as explicit JSON null.
- Every serialized record ends in exactly one line feed.

## Dependencies

The event record depends only on foundational model types and the C++20 standard library.
The serializer implementation additionally depends on nlohmann/json, but its
public header does not expose that dependency. The module does not depend on
runtime state, an event queue, scheduling policy, networking, Bosch
definitions, FMI, MATLAB, Simulink, or GUI libraries.

## Failure behavior

- Negative event tick throws `std::invalid_argument`.
- A self-referential or forward causal sequence throws
  `std::invalid_argument`.
- An enum value outside the declared phase/type vocabulary throws
  `std::logic_error` during serialization.
- Rejected construction creates no partial canonical event.

## Example

```cpp
const Event event{
    25,
    EventPhase::Scheduling,
    EventSequence{7},
    EventType::JobStart,
    EventEntityRefs{
        .task_id = TaskId{2},
        .job_id = JobId{11},
        .resource_id = ResourceId{3},
        .message_id = std::nullopt,
        .vehicle_id = std::nullopt,
    },
    EventSequence{5},
};

const std::string line = serialize_event_json_line(event);
```

Record validation is tested by
[event_test.cpp](../../tests/model/event_test.cpp), and exact output bytes are
tested by [event_json_test.cpp](../../tests/trace/event_json_test.cpp),
including the current message event vocabulary.
