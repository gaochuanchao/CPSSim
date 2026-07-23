# G4 — Results, Plots, and Export

## Goal

Provide a coherent post-run analysis workflow for generic CPSSim experiments and the Bosch reference scenario.

Keep canonical traces authoritative. Derived metrics, plots, and spreadsheets must be reproducible from saved run data.

## Analysis tabs

Keep generic raw views:

```text
Architecture
Timeline
Signals
```

Add:

```text
Results
```

`Signals` remains a low-level signal explorer. `Results` contains derived metrics and scenario-aware plots.

## Generic run metrics

Derive from snapshots/traces where possible:

- completed jobs;
- deadline misses;
- preemptions;
- response-time summary;
- resource utilization;
- busy/idle time;
- message counts;
- message-delay summary;
- event count;
- simulation horizon.

Do not insert report-only data into scheduler or event state.

## Bosch result plots

Initial required plots:

### 1. Lateral error

Show:

- lateral error over time;
- `+0.2 m` and `-0.2 m` threshold lines;
- highlighted critical sections;
- optional deadline-miss markers;
- selectable cursor/range synchronized with Timeline and Events.

### 2. Vehicle/control state

Select from available Bosch observations:

- speed;
- desired lateral acceleration/reference;
- yaw rate;
- steering state;
- critical-section indicator;
- other existing typed signals.

### 3. Resource and timing outcomes

Show:

- resource utilization;
- Ready-queue evolution if available;
- deadline misses over time;
- preemptions over time;
- task response-time distribution or summary.

Do not invent unavailable metrics. Mark unsupported metrics clearly.

## Plot requirements

- use full-resolution detached data as the source;
- visual downsampling may affect drawing only;
- selected ranges must use original data;
- axes show units;
- legends use readable signal names;
- cursor selection integrates with existing shared tick/range selection;
- theme-compatible rendering;
- large traces remain responsive.

## Run result directory

Each completed/saved run uses:

```text
results/<run-id>/
├── manifest.json
├── system.json
├── run-plan.json
├── events.jsonl
├── events.csv
├── signals.csv
├── metrics.json
├── metrics.csv
└── results.xlsx
```

A timestamp-based `run-id` is acceptable initially. Avoid overwriting existing runs.

## Run manifest

Record:

```text
CPSSim version
project name
run ID
creation time
system file/checksum
run-plan file/checksum
policy name
stop tick
scenario kind
Bosch trajectory when applicable
FMU identity/path when applicable
random seed when stochastic models exist
```

Use relative paths where practical.

## Raw export

Authoritative formats:

- canonical events: JSONL;
- tabular events: CSV;
- functional observations: CSV;
- metrics: JSON and CSV;
- copied system/run plan: JSON.

CSV requirements:

- stable column order;
- explicit units in headers or metadata;
- missing optional IDs represented consistently;
- locale-independent decimal formatting;
- UTF-8;
- no presentation sorting unless user explicitly exports a filtered selection.

## Excel export

Create `results.xlsx` as a convenience artifact.

Suggested sheets:

```text
Run Summary
System
Tasks
Resources
Events
Functional Signals
Scheduling Metrics
Control Metrics
```

Rules:

- Excel is not the only saved format;
- large event sets may be split across sheets or omitted with a clear note;
- never silently truncate rows;
- include the raw-file paths in `Run Summary`;
- preserve integer ticks exactly;
- store physical time separately from ticks.

Use a focused library with a stable license and pin its version. Do not add a large dependency before evaluating CSV-first export.

A staged implementation is preferred:

```text
Stage 1: JSONL/CSV + manifest
Stage 2: Excel workbook
```

## Export dialog

Provide:

```text
Scope:
  Complete run
  Selected time range
  Current visible range

Formats:
  Raw JSON/CSV
  Excel workbook
  Plot images (later)

Destination:
  <project>/results/<run-id>/
  [Browse...]
```

Cancel must not create partial output.

Write to a temporary directory and rename only after all required outputs succeed.

## Run comparison

Defer implementation until single-run export is stable.

Planned comparison inputs:

```text
Dedicated baseline
Shared-cloud baseline
Context-aware policy
```

Comparison should align plots by physical time and report configuration differences.

## Suggested task split

### G4.1 — Derived metrics library

Graphics-independent, read-only analysis target.

Acceptance:

- tested against hand-written small traces;
- formatting changes cannot alter metric values;
- no dependency from core to analysis.

### G4.2 — Run manifest and raw export

Acceptance:

- complete atomic result directory;
- repeatable stable schemas;
- no silent truncation;
- project can locate saved runs.

### G4.3 — Generic Results tab

Acceptance:

- summary metrics;
- synchronized selection;
- no Bosch dependency.

### G4.4 — Bosch result model and plots

Acceptance:

- lateral error plot;
- thresholds;
- critical-section overlay;
- signal registry uses names/units rather than hard-coded view references where possible.

### G4.5 — Excel export

Acceptance:

- workbook opens in common spreadsheet software;
- all exported counts match raw files;
- large-data behavior is explicit and tested.

## Tests

- metrics from a known trace;
- CSV schema and escaping;
- manifest round trip;
- atomic export failure cleanup;
- selected-range boundaries;
- tick precision;
- Bosch plot-series derivation;
- Excel sheet names and row counts;
- no truncation without diagnostic.

Manual checks:

- full Bosch trajectory export;
- open CSV and XLSX;
- compare plotted threshold crossings with raw samples;
- export to a user-selected directory;
- cancel export;
- rerun with an existing run ID.

## Non-goals

- statistical experiment sweeps;
- cloud database export;
- live streaming to external tools;
- automatic paper-figure styling;
- multi-run comparison in the first implementation;
- changing Bosch FMU behavior.
