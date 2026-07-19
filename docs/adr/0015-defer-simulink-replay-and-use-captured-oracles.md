# ADR-0015: Defer Simulink Replay and Use Captured Artifacts as Oracles

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T1, T12, T13, T14, T15, T16

## Context

The original roadmap placed a new MATLAB import/replay helper in T14 between
Bosch trigger encoding and direct FMI integration. CPSSim's objective is a
portable standalone C++ simulator. MATLAB and Simulink are valuable for
producing the initial behavior oracle, but the project owner does not intend
to use them as part of the normal simulator workflow.

T1 already captured scheduler, network, trigger, and FMU-output artifacts.
T12 and T13 can check C++ behavior directly against the scheduler, network,
and trigger CSVs without launching MATLAB.

## Decision

T14 is deferred and is not a prerequisite for T15.

The current validation path is:

```text
captured MATLAB/Simulink artifacts
             |
             v
      C++ conformance tools
             |
             v
       direct FMI execution
```

T15 may proceed after explicit approval and its required FMI-importer ADR.
T16 will compare direct C++ FMU outputs against the captured `fmu_outputs.csv`
artifacts with explicitly documented numerical tolerances.

The existing MATLAB export scripts remain preserved for provenance and
optional oracle regeneration. No new MATLAB helper is implemented now.

## Consequences

Positive:

- normal CPSSim development does not require MATLAB or Simulink;
- effort moves toward the portable direct-FMI path;
- captured references still protect scheduler, network, trigger, and later
  numerical behavior; and
- the architecture keeps MATLAB outside the runtime dependency graph.

Limiting:

- there is no new convenience workflow for replaying arbitrary C++ traces in
  the existing Simulink model;
- functional conformance depends on the provenance and integrity of the
  checked-in reference artifacts; and
- numerical tolerance selection remains required before output comparison.

## Reconsideration

T14 can be resumed later if diagnosing a direct-FMI discrepancy requires a
Simulink replay bridge or if users request MATLAB interoperability. Resuming it
requires explicit approval; it is not silently deleted from the roadmap.
