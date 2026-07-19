# Module: Bosch Captured-Reference Conformance

## Responsibility

Construct the two captured Bosch v10 scenarios on top of the generic
simulator, normalize scheduler/network/trigger observations, compare them
exactly, and report the first divergence.

This module is experiment-specific. It links to `cpssim_core`; the core never
links back to it.

## Inputs and outputs

Inputs are one `BoschReferenceScenario`, the pinned reference root,
`tasks.csv`, `scheduler_events.csv`, `network_events.csv`,
`trigger_events.csv`, and the fixed reference horizon, tick period, and routes.

Output is `ConformanceReport`, containing expected/actual counts, pass/fail,
and a human-readable first divergence.

## Comparison contract

Scheduler observations compare integer tick, lifecycle event type, task
ID/name, task-local job ID, and resource name in processed order.

Network observations compare causal source task/job, destination task,
publication/send/delivery ticks, and in-horizon status in publication order.

Trigger observations compare integer tick, one-based Bosch trigger column,
and stable trigger name in tick/column order.

MATLAB row IDs, floating seconds, offline packet IDs, and network direction
labels are excluded. Trigger columns are compared only through the separate
Bosch adapter projection. See
[ADR-0012](../adr/0012-compare-normalized-matlab-timing-observations.md).

## Dependencies

`cpssim_bosch_reference` uses `cpssim_bosch_adapter` plus public generic model,
allocation, policy, network, and engine interfaces. It does not mutate private
containers.

It has no dependency on MATLAB, Simulink, FMI, the FMU, or a GUI. CSV reading
uses only the C++ standard library.

## Failure behavior

Missing files, changed headers, malformed fields, unknown tasks, or an invalid
scenario throw an exception. The executable reports reference-data errors with
exit code 2, a well-formed timing divergence with exit code 1, and a complete
match with exit code 0.

## Verification

- [bosch_reference_test.cpp](../../tests/conformance/bosch_reference_test.cpp)
- [simulation_engine_test.cpp](../../tests/kernel/simulation_engine_test.cpp)
- CTest `cpssim_bosch_conformance_dedicated`
- CTest `cpssim_bosch_conformance_shared_cloud`
- [Bosch reference experiment](../../experiments/bosch_v10_reference/README.md)
- [Bosch trigger adapter](bosch-trigger-adapter.md)
