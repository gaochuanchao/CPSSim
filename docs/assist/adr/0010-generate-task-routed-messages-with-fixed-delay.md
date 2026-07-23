# ADR-0010: Generate Task-Routed Messages with Fixed Delay

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T6, T7, T10, T11, T12, T13

## Context

T11 must create causal uplink and downlink behavior after relevant task
completions without placing Bosch task names, trigger columns, FMI types, or a
server topology in the simulator core. The MATLAB reference creates one packet
after every completed Sensor or Merger job, sends it one tick after
publication, and receives it after a fixed 80-tick delay.

The portable model already has stable `TaskId`, task-local `JobId`,
`MessageId`, `EventSequence`, `MessageDelivery` and `CausedAction` phases, and
one global deterministic `EventQueue`. It does not yet define message state,
routes, event categories, or the owner of in-flight messages.

ADR-0009 also leaves server identity unresolved. T11 must not silently decide
that every resource is a server or introduce distributed scheduling merely to
represent two causal message paths.

## Decision

### Generic task routes

An immutable `MessageRouteSpec` identifies:

- a source `TaskId`;
- a destination `TaskId`;
- a positive integer send offset; and
- a positive integer fixed delivery delay.

Routes refer to configured tasks and unique source/destination pairs. One
source task may have several destinations; a successful source completion
creates one message per route in ascending destination-task order.

The core does not store `uplink`, `downlink`, Bosch trigger columns, platform
names, network addresses, `ServerId`, or endpoint resources. A later Bosch
adapter can interpret Sensor-to-Estimator as uplink and Merger-to-Actuator as
downlink without changing this task-level causal model.

### Message identity and lifecycle

`FixedDelayNetwork` owns one global one-based `MessageId` allocator and all
runtime `Message` values. IDs follow deterministic dynamic creation order:
processed completion order first, then route destination order. They are not
required to reproduce MATLAB's offline packet-table row numbers; T12 compares
network timing and causal source explicitly.

A message captures:

- its ID;
- source `JobIdentity`;
- destination `TaskId`;
- publication, planned send, and planned delivery ticks;
- the source completion sequence;
- scheduled send/delivery sequences when those candidates are in horizon; and
- lifecycle `PendingSend`, `InFlight`, or `Delivered`.

The lifecycle transitions are:

```text
producer JobFinish -> PendingSend -> MessageSend -> InFlight
                                               -> MessageDelivery -> Delivered
```

The network model alone mutates message lifecycle. Public inspection is
read-only.

### Causal event chain

Only a completion accepted by runtime `Scheduler` publishes messages. A stale
completion candidate cannot create a message.

For each matching route:

1. the valid `JobFinish` sequence is the cause of `MessageSend`;
2. the send is scheduled at `finish tick + send offset` in `CausedAction`;
3. processing that send changes the message to `InFlight`;
4. the `MessageSend` sequence is the cause of `MessageDelivery`; and
5. delivery occurs at `send tick + fixed delay` in `MessageDelivery`.

Send offset and delay must be positive. This intentionally avoids same-tick
fixed-point iteration or a hidden microstep rule. Integer overflow is rejected.

The source task/job and message IDs identify a send event. A delivery event
identifies its destination task and message; the message record and causal
sequence retain the source.

### Horizon behavior

A valid producer completion always creates its message record. A send or
delivery candidate is scheduled only when its planned tick is within the
inclusive engine horizon.

Therefore:

- a message whose send is beyond the horizon remains `PendingSend`;
- a message sent in horizon whose delivery lies beyond it remains `InFlight`;
- only processed send and delivery events enter the canonical trace; and
- planned ticks remain available for later reporting and T12 comparison.

This matches the reference distinction between all created packets and events
that occur within the captured trace.

### Ownership and engine cycle

`FixedDelayNetwork` owns routes, message IDs, and message/in-flight state. The
global `EventQueue` continues to own pending event candidates.

`SimulationEngine`:

- gives accepted job completions to the network model;
- processes `MessageDelivery` before deadline/release/scheduling phases;
- invokes runtime `Scheduler` as before;
- processes `MessageSend` in the final `CausedAction` phase; and
- appends successful network observations to its trace.

`Scheduler`, `SchedulingPolicy`, and `Resource` gain no network responsibility.

### Configuration compatibility

JSON schema version 4 adds a required `message_routes` array. Versions 1–3
remain readable and translate to an empty route collection, preserving their
historical no-network behavior.

## Consequences

Positive:

- message behavior is portable and independent of Bosch/FMI concepts;
- source and destination IDs are stable and validated;
- send and delivery have an explicit two-link causal chain;
- fixed-delay timing remains exact integer arithmetic;
- stale job completions cannot publish duplicate messages;
- one-to-many task routes have deterministic message order; and
- out-of-horizon delivery remains inspectable without becoming trace history.

Limiting:

- only completion-triggered messages exist;
- routes and timing are fixed for one run;
- no payload, size, bandwidth, queueing contention, loss, or randomness exists;
- no receiving task activation or functional-model call occurs;
- message IDs reflect online creation order rather than MATLAB table rows; and
- task endpoints do not define server or resource topology.

## Alternatives considered

### Hard-code Sensor and Merger names

Rejected because the core must remain reusable and Bosch-specific mapping
belongs in a later adapter.

### Route by ResourceId or introduce ServerId

Rejected because task placement and server/network topology are different
decisions. T11 does not need a server model to reproduce the two causal paths.

### Generate send and delivery together at job completion

Rejected because delivery should be caused by an actually processed send, and
the network must represent `PendingSend` and `InFlight` state explicitly.

### Allow zero offset or zero delay

Deferred because it requires same-tick caused-action/delivery iteration and a
clear microstep or fixed-point contract. The reference uses positive values.

### Add message-specific payload variants to Event JSON

Deferred because stable IDs, message state, and causal sequences provide the
current T11 behavior. Payload schema should be introduced only with a concrete
functional-data requirement.

## Validation

Tests must cover model invariants, route validation, deterministic message IDs,
one-to-many route order, send/delivery lifecycle, exact causal sequences,
positive timing, overflow, horizon truncation, stale-completion suppression,
same-tick phase precedence, legacy JSON compatibility, and byte-repeatable
integrated traces.

## Follow-up

[ADR-0011](0011-plan-user-configured-task-channels.md) records the proposed
future evolution from fixed routes to one persistent user-configured channel
per directed task pair. It also proposes zero-delay write-before-read and job
input-snapshot semantics. T11 remains unchanged until that separate channel
task defines and tests zero-delay processing.
