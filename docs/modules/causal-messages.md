# Module: Causal Fixed-Delay Messages

## Responsibility

Create generic task-routed messages from accepted job completions and advance
them through deterministic send and delivery events.

## Public model and owner

- `MessageRouteSpec` is immutable configuration: source task, destination
  task, positive send offset, and positive fixed delay.
- `Message` is a read-only runtime record with source job, destination, exact
  timing, causal sequences, and lifecycle.
- `FixedDelayNetwork` owns sorted routes, the one-based message-ID allocator,
  all messages, and their lifecycle transitions.
- `EventQueue` separately owns pending send and delivery candidates.
- `SimulationEngine` routes accepted events and owns the append-only trace.

The detailed explanation is in
[Simulation semantics](../guide/SIMULATION-SEMANTICS.md#messages), and the
durable decision is
[ADR-0010](../adr/0010-generate-task-routed-messages-with-fixed-delay.md).

## Event contracts

| Event | Required meaning | Cause |
|---|---|---|
| `JobFinish` | Accepted source task/job completion | Existing scheduling cause |
| `MessageSend` | Source task/job/message in `CausedAction` | Source `JobFinish` sequence |
| `MessageDelivery` | Destination task/message in `MessageDelivery` | `MessageSend` sequence |

Only `Scheduler`-accepted completion events publish. Stale completion
candidates do not create messages.

## Invariants

- Routes use known tasks, unique endpoint pairs, and positive integer timing.
- Route order is canonical by source then destination task ID.
- Message IDs are stable, global within one network instance, and one-based.
- Publication precedes send; send precedes delivery.
- Lifecycle is exactly `PendingSend`, `InFlight`, or `Delivered`.
- Timing addition stays inside the signed `Tick` domain.
- Only in-horizon candidates enter the queue and processed trace.
- Public message and route views are read-only.

## Dependencies

The network depends inward on model records and the kernel event queue. It has
no dependency on Bosch trigger definitions, FMI, MATLAB, Simulink, GUI
libraries, resources, server topology, or a scheduling policy.

## Failure behavior

Invalid configuration or message timing throws `std::invalid_argument`.
Tick or ID exhaustion throws `std::overflow_error`. An event that does not
match the expected message identity, lifecycle, tick, sequence, cause, and
endpoint throws `std::logic_error` without completing that transition.

## Verification

- [message_test.cpp](../../tests/model/message_test.cpp)
- [fixed_delay_network_test.cpp](../../tests/network/fixed_delay_network_test.cpp)
- [network_simulation_test.cpp](../../tests/kernel/network_simulation_test.cpp)
- [json_config_test.cpp](../../tests/config/json_config_test.cpp)
- [event_json_test.cpp](../../tests/trace/event_json_test.cpp)
