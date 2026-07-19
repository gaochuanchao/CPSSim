# Module: FMI 2.0 Co-Simulation Adapter

## Responsibility

Load one prepared FMI 2.0 Co-Simulation platform library, own one component,
coordinate its lifecycle, provide typed value-reference access, perform
synchronous communication steps, and propagate statuses.

## Public interface

[`fmi2_importer.hpp`](../../src/cpssim/fmi/fmi2_importer.hpp) declares:

- `Fmi2ModelInfo` for library and model identity;
- `Fmi2Status` and `Fmi2CallResult` for runtime results;
- `Fmi2Lifecycle` for adapter-owned state;
- `Fmi2InitialReal` for parameter writes during initialization; and
- `Fmi2CoSimulation` for initialization, typed access, stepping, termination,
  and cleanup.

## Owned state

The implementation owns the platform library handle, resolved function table,
callback record, FMI component pointer, model metadata, and lifecycle. None of
these objects are owned by `SimulationEngine` or `cpssim_core`.

## Invariants

- all required symbols resolve before construction succeeds;
- no typed access or step occurs outside `Initialized`;
- output arguments change only after a successful FMI read;
- partial initialization frees its component;
- requested Real parameters are applied inside initialization mode;
- termination always frees the component; and
- the component is freed before the shared library closes.

## Dependencies

`cpssim_fmi2_adapter` uses the C++ standard library and the platform dynamic
loader (`dlopen`/`dlsym` on Ubuntu). It does not depend on `cpssim_core`, Bosch
trigger definitions, MATLAB, Simulink, or GUI code.

The build-tree `cpssim_bosch_fmu_linux` test model is compiled from supplied
Bosch source and is not a runtime dependency of the generic adapter.

## Failure behavior

Incomplete metadata and library/symbol loading throw because no usable object
exists. Lifecycle misuse and FMI-returned statuses produce `Fmi2CallResult`.
`Ok` and `Warning` count as completed calls; every other status is preserved
as unsuccessful.

## Verification

- [fmi2_importer_test.cpp](../../tests/fmi/fmi2_importer_test.cpp)
- [FMI selection ADR](../adr/0016-use-a-small-fmi2-co-simulation-loader.md)
- [ADR-0016](../adr/0016-use-a-small-fmi2-co-simulation-loader.md)
- `make fmi-test`
