# Bosch v10 MATLAB Reference

## Purpose

This directory defines the behavior oracle for the first CPSSim
conformance scenarios. It preserves the validated single-vehicle MATLAB
scheduler runs and documents how to export portable scheduler, network,
trigger, and FMU-output traces without changing scheduler semantics.

The checked-in MATLAB v7.3 archives are the current scheduler oracle:

- `simulink/cps_v10_scheduled_dedicated.mat`
- `simulink/cps_v10_scheduled_cloud1.mat`

They were created by commit `01e717e9fa6d9386572a7f6ed550cbdba7861eef`.
The export harness regenerates each scenario and requires exact equality with
the corresponding archive before writing portable files.

The generated CSV files under `dedicated/` and `shared_cloud/` have been
captured with `fmu_executed=1`, pinned in `checksums.sha256`, and validated
row-by-row against `reference_metadata.json`.

The C++ comparison matches every normalized scheduler, network,
and sparse Bosch trigger record in both scenarios. Run it from the repository
root with:

```bash
make conformance
```

The compared fields and intentional ID exclusions are documented in the
[Bosch conformance module](../../docs/modules/bosch-timing-conformance.md). The
sixteen trigger mappings are documented in the
[trigger-adapter module](../../docs/modules/bosch-trigger-adapter.md).

The online functional adapter also drives the supplied Bosch C implementation
directly from the C++ event engine. Both functional-output tables match across
all 150,001 rows,
and each online result exactly matches offline canonical-trace replay. Run:

```bash
make functional-test
```

The call order and typed boundary are documented in the
[online-functional module](../../docs/modules/online-functional-interaction.md)
and [ordering ADR](../../docs/adr/0017-order-online-functional-observation-before-same-tick-actions.md).
The measured errors and tolerance rule appear below.

### Numerical conformance

Tick, Integer, Boolean, signal name, row count, and online/offline replay use
exact equality. Real values pass when
`absolute_error <= 1e-12 + 1e-9 × |expected|`.

| Scenario | Maximum absolute error | Maximum relative error |
|---|---:|---:|
| Dedicated resources | `4.9960036108132044e-16` | `5.0193016215114218e-15` |
| Shared cloud | `4.9960036108132044e-16` | `5.0101277533785536e-15` |

## Reference configuration

- Trajectory: `examples/example_v_10`
- Vehicle speed at time zero: 10 m/s
- Captured interval: 0 through 15 seconds, inclusive
- Integer scheduler tick: `1e-4` seconds (0.1 ms)
- Stop tick: 150000
- Trace samples: 150001
- Releases: synchronous at tick 0
- Relative deadlines: equal to periods
- Execution times: deterministic
- Scheduling: fully preemptive fixed priority on each resource
- Priority convention: a smaller number has higher priority
- Equal-priority behavior: no preemption
- Ready-queue order: priority, release tick, task ID, job ID
- Network send offset: 1 tick after Sensor or Merger completion
- Network delay: fixed at 80 ticks (8 ms)
- Random scheduler/network samples: none
- MATLAB master seed for the export harness: 0 using `twister`
- FMU noise standard deviations: zero

The complete task table, trigger-column mapping, expected counts, source
hashes, and comparison policy are in `reference_metadata.json`.

Supporting source files are organized as follows:

- `LateralMotionControl.fmu` remains at the repository root because the
  Simulink model stores that relative filename;
- the challenge paper and presentation are under `resources/`;
- `resources/README.md` explains the purpose of the supplied resource files.

## Scenarios

| Scenario | Resource mapping | Scheduler events | Starts | Finishes | Trigger entries | Trigger differences | Deadline misses |
|---|---|---:|---:|---:|---:|---:|---:|
| `dedicated` | One uniprocessor per task | 21762 | 7256 | 7250 | 22005 | 0 | 0 |
| `shared_cloud` | Estimator, Controller, Feedforward, and Merger share `cloud_cpu_1`; vehicle tasks remain dedicated | 21759 | 7253 | 7250 | 22002 | 12003 | 0 |

Both scenarios release 7256 jobs and generate 3750 causal network packets.
There are 3750 sends and 3749 receives within the 15-second trace. The final
uplink receive falls beyond the experiment horizon.

The shared-cloud baseline has no preemptions: synchronous cloud releases and
the selected priorities cause Estimator, Controller, Feedforward, and Merger
to execute in that order before the next release. Three cloud jobs released
at the final stop tick are never dispatched because the trace contains no
execution interval after that tick. This accounts for the three fewer start
and trigger events in that scenario.

## Prerequisites

The full export requires:

- Windows, because the checked-in FMU contains a Win64 binary;
- MATLAB with Simulink and FMI 2.0 Co-Simulation support;
- a MATLAB release able to open `simulink/cps_single_vehicle_v10.slx`;
- the repository as the current working directory.

Scheduler-only regeneration does not load the Simulink model, but still
requires MATLAB because the reference implementation uses MATLAB tables,
strings, and timeseries archives.

## Regenerate both scenarios

From the repository root in MATLAB:

```matlab
addpath(fullfile(pwd, "simulink"));
export_cps_v10_reference;
```

Equivalent non-interactive command from a shell whose current directory is
the repository root:

```bash
matlab -batch 'addpath(fullfile(pwd,"simulink")); export_cps_v10_reference'
```

To regenerate and verify scheduler artifacts without running Simulink or the
FMU:

```bash
matlab -batch 'addpath(fullfile(pwd,"simulink")); export_cps_v10_reference("",false)'
```

By default, the harness refuses to replace an existing generated artifact.
Golden regeneration must have a documented reason and review. After that
reason is recorded, replacement can be explicitly enabled:

```matlab
export_cps_v10_reference("", true, true);
```

The Boolean arguments mean `runFmu` and `allowOverwrite`, respectively.

## Generated artifacts

The harness writes the following files under both `dedicated/` and
`shared_cloud/`:

- `tasks.csv`: immutable task configuration and resource mapping;
- `jobs.csv`: release, deadline, execution, start, finish, and outcome fields;
- `scheduler_events.csv`: append-order release, start, resume, preempt, finish,
  and deadline-miss events;
- `network_events.csv`: causal Sensor/Merger publications and their send and
  receive ticks;
- `trigger_events.csv`: sparse tick/column representation of the sixteen Bosch
  FMU trigger inputs;
- `summary.csv`: expected event and packet counts plus FMU execution status;
- `fmu_outputs.csv`: selected functional/physical outputs when `runFmu=true`.

The selected FMU outputs are:

- `lateral_error`;
- `actuator_command`;
- `rolling_real`;
- `rolling_remote`;
- `violation_counter`;
- `critical_section`.

These are the six `timeseries` outputs already connected to To Workspace
blocks in `simulink/cps_single_vehicle_v10.slx`. Functional conformance compares
their Real values using `1e-12 + 1e-9 × |expected|`; Integer and Boolean values
compare exactly.

## Validate source artifacts

From the repository root:

```bash
sha256sum -c experiments/bosch_v10_reference/checksums.sha256
```

Every entry must report `OK`. A mismatch means the reference inputs or oracle
changed and the golden files must not be regenerated silently.

After a full MATLAB export, confirm that both scenario directories contain the
expected files:

```matlab
assert(isfile(fullfile( ...
    "experiments", "bosch_v10_reference", ...
    "dedicated", "scheduler_events.csv")));
assert(isfile(fullfile( ...
    "experiments", "bosch_v10_reference", ...
    "dedicated", "fmu_outputs.csv")));
assert(isfile(fullfile( ...
    "experiments", "bosch_v10_reference", ...
    "shared_cloud", "scheduler_events.csv")));
assert(isfile(fullfile( ...
    "experiments", "bosch_v10_reference", ...
    "shared_cloud", "fmu_outputs.csv")));
```

The export harness itself verifies regenerated `cfg`, tasks, jobs, scheduler
event order, per-tick execution timeline, trigger matrix, and network log
against the checked-in `.mat` oracle using exact MATLAB equality.

## Time-quantum note

Figure 4 in the RTAS 2026 paper labels the FMU step as 1 ms. The checked-in
FMU description, Simulink model, `example_v_10` trajectory, and MATLAB timing
implementation consistently use 0.1 ms. The paper states that the GitHub
repository description prevails if it disagrees with the paper, so this
reference uses the repository value of `1e-4` seconds.

## Known limitations

- The CSV artifacts were generated in a user-provided Windows MATLAB/Simulink
  environment, but its exact MATLAB release was not recorded. The artifacts
  were subsequently validated in the Ubuntu workspace.
- The checked-in FMU contains only a Win64 binary. Scheduler-only regeneration
  is portable across MATLAB hosts, but the full functional run is not.
- The 15-second capture is a representative prefix of the supplied 150-second
  `example_v_10` trajectory, not a full-lap challenge result.
- Real output comparisons use `1e-12 + 1e-9 × |expected|`; measured errors are
  documented in the generated-reference validation results.

## Linux FMI execution note

The importer does not make the archived FMU platform-neutral: its packaged binary is
still Win64 only. For importer tests, CMake compiles the tracked supplied
`LateralMotionControl/sources/LateralMotionControl.c` into a build-tree Linux
shared library and loads that generated library through `cpssim_fmi2_adapter`.
The original `.fmu`, DLL, sources, and pinned checksums are not changed. See
the [FMI module](../../docs/modules/fmi2-co-simulation.md).
