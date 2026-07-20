# Results and Export

CPSSim derives run results from one detached, immutable simulation snapshot.
The Results tab and every exporter consume the same `RunResult` metrics and
typed signal model; neither path adds report-only state to the simulator.

## Result directory

Each export is published as `results/<run-id>/` (or below a directory selected
in the export dialog):

```text
manifest.json
system.json
run-plan.json
events.jsonl
events.csv
signals.csv
metrics.json
metrics.csv
results.xlsx       # when Excel is selected
```

Raw JSON/CSV is mandatory and authoritative. Excel is a convenience view of
the same rows. CPSSim rejects an existing run ID, writes every file below a
unique temporary sibling directory, and renames that directory only after all
requested files close successfully. A failure removes that temporary output.

`manifest.json` records schema version 1, the CPSSim/project/run identity,
creation time, scenario and optional Bosch/FMU provenance, applied policy and
stop tick, plus labeled FNV-1a checksums of the exact exported system and run
plan text. These checksums detect reproducibility mismatches; they are not
cryptographic signatures.

## Stable raw schemas

Event exports retain canonical sequence order. `events.csv` uses:

```text
sequence,tick,time_seconds,type,phase,task_id,job_id,resource_id,message_id,vehicle_id,cause_sequence
```

An unavailable optional ID is an empty CSV cell and JSON remains the canonical
`null` representation. Integer ticks and IDs are never converted through
floating point. Physical seconds are a separate locale-independent boundary
column.

`signals.csv` contains one row per full-resolution typed sample and includes
tick, physical time, scalar type, source identity, path, display name, unit,
and value. Drawing may downsample, but export does not.

Generic metrics include counts, exact response/message tick summaries, and
resource busy/idle/utilization. An unmatched timing pair or zero observed
resource ticks is serialized as unavailable rather than as a fabricated zero.
For a selected-range export, endpoint ticks are inclusive; counters that
cannot be reconstructed safely from the selected trace, such as partial-range
resource utilization, remain unavailable.

## Excel policy

The workbook uses the pinned BSD-2-Clause libxlsxwriter release v1.2.4 and
pinned zlib 1.3.1 sources. Stable sheets are Run Summary, System, Tasks,
Resources, Events, Functional Signals, Scheduling Metrics, and Control Metrics
when Bosch-derived rows are available.

Excel stores large integers imprecisely as numeric cells, so ticks and IDs are
written as text while physical time and real signals remain numeric. Detail
tables exceeding Excel's 1,048,576-row limit split deterministically into
`Events 2`, `Functional Signals 2`, and so on; raw files remain complete.

## Presentation and ownership

The Results tab is read-only. Resource rows and plot clicks update the existing
runtime selection. Bosch views locate lateral error and critical-section data
by typed signal identity, show the +/-0.2 m bounds, critical intervals,
deadline-miss markers, and any already-selected control signals. Missing
signals produce an explicit diagnostic.

Workspace schema 3 adds Results panel visibility and the active Results tab.
Schema 1 and 2 workspaces migrate to safe defaults. Runtime traces, metrics,
ticks, export dialogs, and run results are never persisted in `workspace.json`.
