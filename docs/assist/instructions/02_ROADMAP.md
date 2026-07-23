# Roadmap

This page states the current development direction. Detailed designs,
dependencies, and proposed acceptance tests are in the
[future-work guide](../guide/FUTURE-WORK.md). Its F1–F9 labels organize
discussion; they are not approved task numbers.

## Current baseline

CPSSim has a tested C++20 foundation with integer ticks, deterministic events,
runtime periodic jobs, resource allocation, fixed-priority scheduling across
independent exclusive resources, causal fixed-delay messages, captured Bosch
timing/trigger conformance, FMI 2.0 Co-Simulation loading, online functional
interaction, and an optional snapshot-based GUI with an explicit run plan,
architecture graph, scheduling timeline, and functional plots.
The terminal interface now provides registered interactive/direct commands and
a shared application service for supplied Bosch trajectories; F1 still owns
general JSON experiment execution and trace/manifest output.

The exact behavioral contract is in
[Simulation semantics](../guide/SIMULATION-SEMANTICS.md). Current module
ownership is in [Module interactions](../MODULE-INTERACTIONS.md).

No next implementation task is selected merely by this ordering.

## Near-term usability

Recommended sequence:

1. F1: make `cpssim_cli` run an experiment, accept an explicit allocation
   plan, and write canonical JSON Lines plus a run manifest;
2. F2: derive reproducible utilization, response-time, deadline, and
   preemption reports without changing canonical events; and
3. F3: improve GUI usability with workspace preferences, navigation, export,
   and measured performance work while preserving detached snapshots.

Step-one-physical-tick needs a separate decision because quiet ticks currently
do not require an engine event.

## General task dataflow

F4 introduces one user-configured directed channel per task pair, one source
output port and destination input port per channel, data versions, and explicit
write/delivery/read timing. Resolve the open questions in
[ADR-0011](../adr/0011-plan-user-configured-task-channels.md) before coding.

Keep the current fixed-delay route behavior compatible with the Bosch
conformance oracle.

## Broader workload and platform models

After the basic channel lifecycle is stable:

- F5 may add activation types, overrun/deadline policies, trace-driven or
  seeded execution demand, and explicit failure models;
- F6 may add server identities, one scheduling domain per server, partitioned
  multicore allocation, and later a separate shared-capacity resource model;
- F7 may add payloads, deterministic network queues, contention, variable
  delay, and seeded loss; and
- F9 may add installation, hosted CI, coverage, benchmarks, release packaging,
  and broader platform validation throughout these stages.

Global scheduling, migration, and fractional capacity require explicit
semantics rather than flags on the current exclusive `Resource`.

## Deferred co-simulation and research work

F8 covers portable FMU archive extraction, FMI event iteration, rollback,
multiple functional models, and possible superdense time. Add a distinct
`Microstep` only if a concrete model requires repeated evaluation at one tick;
`EventSequence` remains an identity.

Optional Simulink replay remains a diagnostic correctness route under
[ADR-0015](../adr/0015-defer-simulink-replay-and-use-captured-oracles.md), not
the simulator architecture.

Other research directions—learning-based policies, SUMO coupling, cooperative
perception, AI latency/accuracy/energy models, multiple cloud endpoints, and
distributed orchestration—need a concrete experiment and validation oracle
before becoming implementation tasks.

## Selecting the next task

Before implementation:

1. select one smallest behavior from the future-work guide;
2. identify its state owner and compatibility boundary;
3. resolve required ADR decisions;
4. define focused tests and completion evidence; and
5. obtain explicit approval for that scope.

Do not combine later stages or begin the next proposal automatically.
