# Module: Bosch v10 Trigger Adapter

## Responsibility

Project generic canonical events into the sixteen sparse Boolean pulses used
by the Bosch v10 FMU, and serialize those pulses in the captured CSV schema.

## State and public interface

The module owns no mutable runtime state. Its public types and functions are in
[`trigger_encoder.hpp`](../../src/cpssim/bosch/trigger_encoder.hpp):

- `BoschTrigger` names the sixteen adapter outputs;
- `BoschTriggerEvent` stores one tick and trigger;
- column/name functions define the stable external identity;
- `encode_bosch_v10_triggers()` performs the projection; and
- `serialize_bosch_v10_trigger_csv()` performs deterministic export.

## Dependency boundary

`cpssim_bosch_adapter` depends on `cpssim_core`. The core does not depend on
this module. MATLAB, Simulink, FMI, and the Bosch FMU are not linked or invoked.

## Semantics

First dispatch activates, successful completion finishes, Sensor communication
is uplink, and Merger communication is downlink. Resume does not reactivate.
Output is sorted by tick/column and duplicate Boolean cells collapse.

Mapped events with an incorrect phase, missing task reference, or unsupported
v10 task are rejected. See
[ADR-0014](../adr/0014-encode-bosch-v10-triggers-as-an-adapter-projection.md).

## Verification

- [trigger_encoder_test.cpp](../../tests/bosch/trigger_encoder_test.cpp)
- [bosch_reference_test.cpp](../../tests/conformance/bosch_reference_test.cpp)
- [Bosch reference experiment](../../experiments/bosch_v10_reference/README.md)
- CTest scenarios `cpssim_bosch_conformance_dedicated` and
  `cpssim_bosch_conformance_shared_cloud`
