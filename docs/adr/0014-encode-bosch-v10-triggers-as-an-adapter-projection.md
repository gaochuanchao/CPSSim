# ADR-0014: Encode Bosch v10 Triggers as an Adapter Projection

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T6, T11, T12, T13, T15, T16

## Context

The Bosch lateral-motion-control FMU exposes sixteen Boolean trigger inputs:
activation and finish inputs for six tasks plus send and receive inputs for two
network directions. These names and columns are specific to that FMU. Adding
them to `Event`, `TaskSpec`, `Message`, or `SimulationEngine` would make the
portable simulator core depend on the first validation scenario.

The captured MATLAB trigger matrix is derived from observable scheduler and
network behavior:

- first dispatch produces task activation;
- successful completion produces task finish;
- Sensor message send/delivery produces uplink send/receive;
- Merger message send/delivery produces downlink send/receive; and
- a resumed job does not produce another activation.

T13 must preserve those rules for later direct FMU execution without making
MATLAB a runtime dependency.

## Decision

T13 adds a separate `cpssim_bosch_adapter` library that depends on
`cpssim_core`. The core does not depend on the adapter.

The adapter projects a completed canonical event trace into sparse
`BoschTriggerEvent` records. Each record contains only integer `Tick` and one
`BoschTrigger` value. The sixteen external columns and names are returned by
explicit mapping functions; enum declaration order is not the FMU contract.

### Event mapping

| Generic event | Required task meaning | Bosch output |
|---|---|---|
| `JobStart` | Task 1–6 first dispatch | Corresponding activation trigger |
| `JobFinish` | Task 1–6 successful completion | Corresponding finish trigger |
| `MessageSend` | Source task Sensor or Merger | Uplink or downlink send |
| `MessageDelivery` | Destination task Estimator or Actuator | Uplink or downlink receive |

`JobRelease`, `JobPreempt`, `JobResume`, and `DeadlineMiss` are ignored. This
ensures preemption/resumption does not falsely reactivate an FMU task.

Mapped events must have the canonical phase used by the core and a relevant
task ID. Unknown task IDs, missing references, or inconsistent phases are
rejected rather than silently producing a different FMU input.

### Sparse Boolean semantics

The adapter sorts output by smaller tick and then smaller external trigger
column. Duplicate `(tick, trigger)` records collapse to one row because the
reference matrix stores one Boolean value per tick and column.

CSV serialization writes the captured four-column schema:

```text
eventTick,eventTimeSec,triggerColumn,triggerName
```

`eventTick` remains canonical. `eventTimeSec` is derived exactly for the fixed
Bosch v10 quantum of 0.0001 seconds using integer decimal formatting; it is not
used for comparison or simulator ordering.

Golden conformance compares integer tick, column, and name exactly. It does
not launch MATLAB, Simulink, or the FMU.

## Consequences

Positive:

- T15 can apply the same tested trigger vocabulary to the FMU;
- the generic core remains independent of Bosch and FMI concepts;
- all trigger behavior is derived from accepted canonical observations;
- output is deterministic and compact; and
- the captured trigger CSVs provide automated correctness checks without
  requiring MATLAB.

Limiting:

- the adapter deliberately knows the six v10 task IDs and sixteen FMU inputs;
- it encodes Boolean pulses only, not feedforward or velocity inputs;
- it consumes a completed trace rather than applying triggers online;
- another Bosch configuration with different task IDs needs another explicit
  mapping decision; and
- T13 does not load or execute the FMU.

## Validation

- unit tests cover all sixteen mappings and ignored lifecycle events;
- output sorting, duplicate Boolean collapse, invalid shapes, and exact CSV
  decimal formatting are tested;
- dedicated and shared-cloud rows must match every captured trigger event; and
- normal Debug, Release, sanitizer, Clang, and clang-tidy checks must pass.

