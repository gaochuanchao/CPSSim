# Module: Foundational Model Types

## Responsibility

Provide portable value types shared by later model and kernel modules without
owning simulation state or depending on adapters.

## Public interface

- [identifiers.hpp](../../src/cpssim/model/identifiers.hpp) is included as
  `cpssim/model/identifiers.hpp` and provides `TaskId`, task-local `JobId`,
  composite `JobIdentity`, `ResourceId`, `MessageId`, `VehicleId`, and
  `EventSequence`.
- [time.hpp](../../src/cpssim/model/time.hpp) is included as
  `cpssim/model/time.hpp` and
  provides `Tick`, `PhysicalDuration`,
  `duration_to_ticks`, and `ticks_to_duration`.
- [categories.hpp](../../src/cpssim/model/categories.hpp) is included as
  `cpssim/model/categories.hpp` and provides `EventType` and `JobLifecycle`.
  `EventPhase` also lives in this shared category header.

## Owned state

This module owns no mutable global or simulator runtime state. Each identifier
owns one private integer value that cannot be changed through its public
interface. Conversion functions use only their arguments and have no hidden
state.

## Invariants

- Identifier domains are distinct C++ types and do not implicitly accept raw
  integers.
- Identifier values cannot be modified through their public interface.
- Identifiers compare deterministically by their stored unsigned value, but
  only with the same identifier type.
- `JobIdentity` combines the producing `TaskId` and task-local `JobId` when a
  runtime collection needs an unambiguous job reference.
- Canonical logical time is `std::int64_t` ticks.
- Canonical event time will not be represented by floating-point seconds.
- A physical tick period is strictly positive.
- Physical/tick conversion accepts only nonnegative, exactly representable
  values.
- Conversion never silently rounds or overflows.
- `JobLifecycle` values are mutually exclusive; deadline miss is an event, not
  a lifecycle state.
- `EventPhase` names semantic processing groups but does not establish their
  queue precedence.

## Lifecycle semantic contract

The enum declaration itself does not perform transitions. The baseline
implemented lifecycle contract is:

- `Ready` means released and eligible but not executing;
- `Running` means assigned to exactly one resource and receiving execution;
- `Completed` means remaining execution is zero and the job is terminal;
- `Cancelled` is a reserved terminal state whose entry semantics are not yet
  defined.

The active path begins when a release creates a `Ready` job, then continues
`Ready -> Running -> Completed`, with `Running -> Ready` on preemption. The
scheduler pairs the first `Resource::start_job` with `JobStart` and a later
start with `JobResume`, so `JobState` keeps `has_started` history beyond the
lifecycle enum. `DeadlineMiss` does not automatically change lifecycle.

The transition timing and forbidden behavior are summarized in
[Simulation semantics](../guide/SIMULATION-SEMANTICS.md). The
[category tests](../../tests/model/categories_test.cpp) enforce scoped types,
while the [runtime-state tests](../../tests/model/runtime_state_test.cpp)
enforce state changes and cross-record invariants.

## Dependencies

Only C++20 standard-library headers. This module must not depend on the kernel,
scheduling policies, networking, Bosch adapters, FMI, MATLAB, Simulink, or GUI
libraries. The dependency boundary is defined by
[the architecture](../instructions/01_ARCHITECTURE.md#7-dependency-rule).

## Failure behavior

- Invalid period, negative input, or a duration that is not an exact multiple
  of the period throws `std::invalid_argument`.
- Tick-to-duration overflow throws `std::overflow_error`.
- Identifier construction permits the full unsigned 64-bit range, including
  zero. Configuration validation enforces uniqueness and its JSON boundary rejects negative or
  out-of-range values; see the
  [experiment-configuration module](experiment-configuration.md#invariants).
- Identifiers do not yet provide hashing, parsing, or serialization.
- Identifiers are serialized only as fields within canonical event JSON Lines;
  standalone identifier parsing remains absent.

## Example

```cpp
using namespace std::chrono_literals;

const cpssim::TaskId task_id{1};
const auto period = std::chrono::duration_cast<cpssim::PhysicalDuration>(100us);
const auto horizon = std::chrono::duration_cast<cpssim::PhysicalDuration>(15s);
const cpssim::Tick stop_tick = cpssim::duration_to_ticks(horizon, period);
```

This produces `stop_tick == 150000` without using floating-point time.
