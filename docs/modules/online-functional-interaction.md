# Module: Online Functional Interaction

## Responsibility

Connect integer-tick canonical events to a replaceable functional model,
validate its lifecycle and observations, record an append-only typed trace,
and support deterministic offline replay.

## Public interfaces

- [`functional_model.hpp`](../../src/cpssim/functional/functional_model.hpp)
  defines typed named observations and the backend-independent model contract.
- [`functional_runtime.hpp`](../../src/cpssim/functional/functional_runtime.hpp)
  defines validated online coordination and canonical-trace replay.
- [`scheduling_policy.hpp`](../../src/cpssim/policy/scheduling_policy.hpp)
  provides the read-only observation hook.

## Ownership

`SimulationEngine` optionally owns one `FunctionalRuntime` but not the
`FunctionalModel` supplied by its caller. The runtime owns the functional
trace. A concrete model owns all external state and environment inputs.
`SchedulingPolicy` may cache observation-derived private state only.

For GUI runs, `GuiSimulationSession` owns a recreatable model factory and
optional presentation-only signal registry. `SimulationController` owns each
fresh model instance, and detached snapshots copy the validated observation
trace. `GuiSignalCache` owns only derived scalar series; Reset or Apply never
reuses an initialized external model. See [ADR-0021](../adr/0021-recreate-functional-models-and-copy-observations-for-gui-runs.md).

## Ordering invariants

- canonical time remains integer `Tick`;
- observation `t` is recorded before scheduling at `t`;
- actions accepted at `t` first affect observation `t + 1`;
- observations are consecutive and signal names are unique per row;
- at most one ordered action batch is applied at an event tick;
- the stop tick is inclusive; and
- offline replay uses the same runtime ordering as online execution.

## Bosch specialization

`BoschFmi2FunctionalModel` owns trajectory sampling, all v10 parameter and
value-reference mappings, trigger pulses, FMI lifecycle, and selected-output
schema. It depends on the generic core, trigger adapter, and FMI adapter. The
generic core does not depend on it.

`load_bosch_example_trajectory()` is the Bosch-side file boundary for the
supplied `example_v_*` directories. It reads all six CSV files in row lockstep,
converts the decimal time vector to consecutive integer ticks exactly, checks
finite position/feedforward/velocity values and positive velocity, and retains
only the three FMI environment inputs. The position vectors are reference
data, not v10 FMU inputs.

The `cpssim_bosch_example` application combines that loader with either
validated single-vehicle reference allocation (`dedicated` or
`shared_cloud`). The example package does not define deadlines, offsets,
priorities, or one required allocation, so the application names this run plan
explicitly rather than treating it as a Bosch-mandated configuration.

## Verification

- [functional_runtime_test.cpp](../../tests/functional/functional_runtime_test.cpp)
- [simulation_engine_test.cpp](../../tests/kernel/simulation_engine_test.cpp)
- [bosch_fmi2_functional_model_test.cpp](../../tests/bosch/bosch_fmi2_functional_model_test.cpp)
- [example_data_test.cpp](../../tests/bosch/example_data_test.cpp)
- [bosch_functional_reference_test.cpp](../../tests/conformance/bosch_functional_reference_test.cpp)
- [signal_series_test.cpp](../../tests/gui/signal_series_test.cpp)
- [simulation_controller_test.cpp](../../tests/gui/simulation_controller_test.cpp)
- [Simulation semantics](../guide/SIMULATION-SEMANTICS.md#functional-model-order)
- [ADR-0017](../adr/0017-order-online-functional-observation-before-same-tick-actions.md)
- `make functional-test`
- `make bosch-example`
- `make bosch-examples`
