# Goal 5 — Bosch Editing, Fast Execution, Flexible Analysis Layout, and Completed-Run Results

## 1. Purpose

Refine CPSSim after Goals 1–4 and Extended Goal 2.

This goal combines five related improvements:

1. Make saved Bosch example projects editable within the Bosch FMU compatibility boundary.
2. Complete the Bosch architecture visualization with missing cloud-side functional dependencies.
3. Add a fast execution mode with configurable event or tick batches.
4. Refine the center workspace and Resources panel.
5. Redesign Results and plotting around a completed-run lifecycle.

The implementation must preserve:

- scheduler semantics;
- canonical event ordering;
- message timing;
- FMI stepping;
- Bosch trigger encoding;
- Bosch numerical behavior;
- existing conformance results;
- CLI behavior;
- project persistence;
- DPI behavior.

---

# 2. Scope

## Included

- editable Bosch-compatible saved projects;
- protected Bosch task identities;
- Bosch baseline/modified status;
- presentation-only functional-dependency arrows;
- distinct functional/network/assignment edge styles;
- Live and Fast execution modes;
- event-count and tick-count batch units;
- user-configurable batch sizes;
- lightweight Fast-mode progress;
- upper and lower center tab groups;
- right-click tab movement between groups;
- existing draggable center splitter;
- one Resources table with inline utilization bars;
- compact Results summary;
- completed-run-only Results calculation;
- non-modal interactive Plot Visualizer;
- selectable signals and plot settings;
- Bosch overlays;
- workspace migration and persistence;
- performance and regression tests.

## Excluded

- Matplot++ integration;
- worker-thread simulation;
- full Dear ImGui docking;
- tab drag-and-drop;
- unrestricted Bosch task creation/deletion;
- new schedulers;
- new network semantics;
- multi-run comparison;
- parameter sweeps;
- multi-vehicle Bosch support.

Matplot++ may be considered later for publication-quality figure export. It is not part of this goal.

---

# Part A — Editable Bosch-compatible projects

## 3. Current issue

The Bosch wizard creates a real project, but saved Bosch projects remain read-only because the editable system draft is currently enabled only for generic projects.

A Bosch project should be editable, but only where changes remain compatible with the fixed Bosch FMU trigger interface.

## 4. Explicit edit policy

Replace scenario-name checks in GUI rendering with an explicit edit policy.

Suggested values:

```text
Generic
BoschCompatible
ReadOnlyAdapter
```

The policy belongs in project/application logic, not in ImGui view code.

## 5. Bosch-compatible edits

Allow:

- resource renaming;
- adding resources;
- dependency-aware resource deletion;
- task period;
- task deadline;
- task offset;
- task priority;
- deterministic execution profiles;
- task-resource assignments;
- stop tick;
- currently supported scheduling policy;
- send offset and delay for the two existing Bosch network routes.

## 6. Protected Bosch structure

Protect:

- task IDs 1–6;
- task names:
  - Sensor;
  - Estimator;
  - Controller;
  - Feedforward;
  - Merger;
  - Actuator;
- adding Bosch FMU tasks;
- deleting Bosch FMU tasks;
- duplicating Bosch FMU tasks;
- changing the endpoints of Bosch network routes;
- deleting the required Bosch network routes;
- removing trajectory/scenario metadata required to reconstruct the Bosch runtime.

Show a clear explanation near protected controls:

```text
This task identity is fixed by the Bosch FMU interface.
Timing, execution profile, and allocation remain editable.
```

Invalid protected changes must be rejected before Apply and restart.

## 7. Baseline and modified status

A new Bosch project should show:

```text
Bosch reference baseline
```

After applying relevant changes:

```text
Modified Bosch experiment
```

Determine status using canonical baseline specifications or checksums, not display strings.

A modified Bosch project remains:

- a Bosch project;
- loadable through the Bosch runtime resolver;
- eligible for Bosch analysis and overlays;
- editable under the Bosch-compatible policy.

Pinned Bosch conformance tests continue to use pinned reference inputs.

---

# Part B — Complete Bosch architecture

## 8. Missing cloud dependencies

The current Bosch architecture shows only network routes:

```text
Sensor → Estimator
Merger → Actuator
```

The following cloud-side functional dependencies must also appear:

```text
Estimator → Controller
Controller → Merger
Feedforward → Merger
```

## 9. Presentation-only dependencies

Do not encode these as `MessageRouteSpec`.

Doing so would incorrectly add:

- network messages;
- delays;
- message-send events;
- message-delivery events;
- altered simulation behavior.

Introduce a presentation-only type, for example:

```text
GuiFunctionalDependency
- source_task_id
- destination_task_id
- optional label
```

Bosch-specific dependency construction remains in the Bosch adapter/application layer.

## 10. Edge styles

Recommended legend:

```text
Solid arrow   Functional dependency
Dashed arrow  Network route
Thin/dim line Task assignment to resource
```

Network routes may show delay information.
Functional dependencies must not show a network delay.

Selecting an edge should identify its kind in Runtime Inspector or equivalent presentation details.

---

# Part C — Live and Fast execution

## 11. Toolbar controls

Add:

```text
Run mode   [Live ▼]
```

Modes:

```text
Live
Fast
```

When Fast is selected:

```text
Batch unit [Events ▼]
Batch size [1000]
```

Units:

```text
Events
Ticks
```

Defaults:

```text
Run mode: Live
Event batch size: 1000
Tick batch size: 1000
Selected Fast unit: Events
```

Keep separate saved values for Events and Ticks.

Add:

```text
Reset batch settings
```

## 12. Validation

Batch sizes are positive integers.

Suggested UI limits:

```text
minimum: 1
default: 1000
soft maximum: 1,000,000
```

The implementation must remain responsive for very large values because an internal wall-clock budget limits each GUI-frame batch.

Batch configuration is presentation/execution pacing state, not semantic run-plan state.

Persist it in workspace state.

## 13. Live mode

Live mode preserves the current interactive behavior:

- small-step advancement;
- continuous refresh of visible views;
- suitable for demonstrations and debugging.

Do not alter Live-mode simulation semantics.

## 14. Fast mode

Fast mode remains cooperative and single-threaded.

Do not add a worker thread in this goal.

Conceptual loop:

```text
while not finished
  and configured event/tick budget not reached
  and wall-clock budget not reached:
      step_to_next_event()
```

Stop when any condition is met:

- event batch count reached;
- tick target reached or passed;
- internal wall-clock budget reached;
- simulation finished;
- controller must safely return to process GUI commands.

Recommended internal wall-clock budget:

```text
25 ms per GUI frame
```

The wall-clock budget may remain internal.

### Event batching

For:

```text
Events, 1000
```

process at most 1000 next-event transitions before returning, subject to the wall-clock limit.

### Tick batching

For:

```text
Ticks, 1000
```

use:

```text
target_tick = batch_start_tick + 1000
```

Continue until the engine reaches or passes that tick, subject to the wall-clock limit.

Do not invent intermediate ticks or events.

## 15. Lightweight progress

Separate lightweight progress from full presentation snapshots.

Suggested type:

```text
SimulationProgress
- run_state
- current_tick
- stop_tick
- processed_event_count, when cheap
```

During Fast execution, update only:

- current tick;
- stop tick;
- percentage;
- run state;
- elapsed wall-clock time;
- configured batch unit and size.

Example:

```text
Fast simulation running

Tick:       82,450 / 150,000
Progress:   54.9%
Batch:      Events, 1000
Elapsed:    1.8 s
```

## 16. Presentation snapshots during Fast mode

During Fast mode:

- keep heavy workbench views on the last complete presentation snapshot;
- do not copy the complete event trace every GUI frame;
- do not copy complete functional observations every GUI frame;
- do not rebuild the timeline, event table, signals, or full resource views every GUI frame.

A complete presentation snapshot may be published when:

- Pause is reached;
- Finish is reached;
- Reset occurs;
- mode switches back to Live;
- an explicit operation requires a coherent snapshot.

This complete presentation snapshot supports Timeline, Events, Signals, Resources, and Runtime Inspector.

It does not automatically trigger Results analysis unless the run is Finished.

## 17. Fast-mode performance measurements

Record:

- elapsed GUI wall-clock duration;
- events per second, when event count is available;
- simulated ticks per second;
- execution mode;
- batch unit;
- configured batch size.

These are observational metrics only.

---

# Part D — Flexible upper/lower center workspace

## 18. Default arrangement

Keep the existing draggable splitter between the two center areas.

Default upper tabs:

```text
Architecture
Scheduling Timeline
Functional Signals
Results
```

Default lower tabs:

```text
Resources
Canonical Events
```

This supports the important workflow:

```text
Scheduling Timeline above
Canonical Events below
```

## 19. Right-click tab movement

Do not implement tab drag-and-drop.

Right-click a tab:

```text
Move to Upper Panel
Move to Lower Panel
```

Rules:

1. Each panel belongs to exactly one group.
2. Moving a panel makes it active in the destination group.
3. If the moved panel was active in the source group, choose a deterministic remaining tab.
4. If a group becomes empty, collapse it.
5. Moving a panel back restores the two-group layout.
6. Hidden panels retain their assigned group.
7. Existing View-menu visibility remains functional.
8. Add:

```text
View → Reset Panel Arrangement
```

## 20. Workspace persistence

Persist:

```text
upper_tabs
lower_tabs
active_upper_tab
active_lower_tab
center_split_ratio
```

Migrate older fields safely, including:

```text
active_analysis_tab
analysis_lower_ratio
resources_events_ratio
active_resource_tab
```

Older project workspaces must continue to load.

---

# Part E — Simplified Resources panel

## 21. Remove nested Resource tabs

Remove:

```text
Resource State
Utilization
```

Use one Resources table.

Columns:

```text
Resource
Running
Ready
Busy ticks
Idle ticks
Utilization
```

## 22. Inline utilization

Render a theme-compatible progress box:

```text
[███████░░░] 72%
```

Requirements:

- show percentage;
- tooltip with busy, idle, and observed ticks;
- click selects the resource;
- selected row stays highlighted;
- unavailable/zero-observation state is explicit;
- do not use warning colors without a defined warning condition.

Remove obsolete resource-subtab state.

---

# Part F — Completed-run Results lifecycle

## 23. Results semantics

Results is a completed-run view.

Do not continuously recompute Results while the simulation is Running or Paused.

The current tick shown before completion comes from lightweight progress, not from `RunResult`.

## 24. Before completion

While Running:

```text
Results

Results are generated after the simulation finishes.

Current tick: 82,450 / 150,000
Run state: Running
```

While Paused:

```text
Simulation paused at tick 82,450.
Results will be generated when the run finishes.
```

Disable:

```text
Open Plot Visualizer...
Export Completed Results...
```

Tooltip:

```text
Available after the simulation finishes.
```

Pausing may refresh Timeline, Events, Signals, Resources, and Runtime Inspector through a complete presentation snapshot, but it must not build final Results.

## 25. Completion transition

When the controller first enters `Finished` for a run generation:

1. publish one complete immutable presentation snapshot;
2. build `RunResult`;
3. calculate generic metrics;
4. calculate Bosch-specific metrics when applicable;
5. calculate plot-series models or references needed by the visualizer;
6. cache the completed result;
7. enable Results;
8. enable Plot Visualizer;
9. enable completed-result export.

Build this once per run generation.

## 26. Completed-result cache

Suggested model:

```text
CompletedRunResult
- run_generation
- immutable RunResult
- RunPerformanceSummary
```

Increment or replace `run_generation` when a new runtime is created.

Invalidate the cached completed result on:

- Reset;
- Apply and restart;
- project replacement;
- project close;
- creation of another runtime;
- applied system or run-plan replacement.

Do not invalidate it merely because:

- tabs move;
- theme changes;
- a panel is hidden;
- the user opens/closes the visualizer.

## 27. Results layout

Results is a compact summary page.

Responsive layout:

```text
+----------------------------+-------------------------------------+
| Run Summary                | Timing and Execution                |
+----------------------------+-------------------------------------+
| Task Response Times                                              |
+------------------------------------------------------------------+
| Bosch Summary, when applicable                                   |
+------------------------------------------------------------------+
```

On narrow width, stack the summary sections vertically.

## 28. Compact Run Summary

Do not stretch Metric/Value over the full panel.

Use bounded/fixed-fit columns:

```text
Metric: about 12–16 text units
Value: fixed-fit
Block width: about 280–340 logical pixels
```

Keep the implementation DPI-safe.

## 29. Timing and Execution

Show:

- messages sent;
- messages delivered;
- paired deliveries;
- undelivered count;
- minimum delivery delay;
- mean delivery delay;
- maximum delivery delay;
- elapsed wall-clock duration;
- events per second;
- simulated ticks per second;
- Live/Fast mode;
- Fast batch unit and size.

## 30. Task response table

Columns:

```text
Task
Completed jobs
Minimum response
Mean response
Maximum response
Deadline
Deadline misses
```

Remove `Total (ticks)` unless another feature depends on it.

Show physical-time equivalents in tooltips where useful.

## 31. Remove resource utilization from Results

Do not display resource utilization in Results.

Keep resource-utilization metrics in analysis/export APIs if already used there.

The Resources panel is the only GUI location for resource utilization.

## 32. Compact Bosch summary

When applicable, show compact values such as:

- threshold violation count;
- maximum absolute lateral error;
- critical-section count or duration;
- deadline misses;
- relevant final/rolling control metrics.

Do not draw large plots directly inside Results.

Buttons:

```text
Open Plot Visualizer...
Export Completed Results...
```

---

# Part G — Plot Visualizer

## 33. Dependency choice

Use ImPlot for the interactive visualizer.

Do not integrate Matplot++ in this goal.

Pin an ImPlot version compatible with the current Dear ImGui version.

Requirements:

- document exact version/tag/commit;
- document MIT license;
- create/destroy ImPlot context beside ImGui context;
- do not compile demo code into production;
- keep CPSSim plot state in CPSSim-owned types;
- verify renderer vertex-offset/index behavior for dense plots.

Design the graphics-independent plot-series model so a future Matplot++ publication exporter can be added without redesigning analysis code.

## 34. Completed-run-only visualizer

The Plot Visualizer is available only when a cached completed result exists.

Before completion:

```text
No completed run is available.
```

Do not build or update visualizer series from a Running or Paused partial run.

## 35. Non-modal window

Open:

```text
Plot Visualizer
```

as a non-modal window.

The user must still be able to interact with:

- Timeline;
- Canonical Events;
- Runtime Inspector;
- Resources;
- Results.

## 36. CPSSim-owned visualizer state

Suggested state:

```text
PlotVisualizerState
- open
- selected signals
- plot lanes
- x_axis_unit
- range mode
- custom range
- y_axis mode/range
- grid
- legend
- line thickness
- markers
- Bosch overlays
```

Persist stable presentation settings in workspace state.

## 37. Signal browser

Support:

- search;
- grouping by signal descriptor path;
- select/unselect;
- reorder selected series;
- clear all;
- Bosch default preset;
- generic functional signals;
- explicit derived timing/scheduling series where already modeled.

Default lane policy:

- group only unit-compatible signals;
- avoid incompatible overlays;
- Boolean signals use a digital/step-style lane.

## 38. Plot controls

Support:

- X-axis in ticks or seconds;
- full-run range;
- selected range;
- custom range;
- automatic Y range;
- manual Y min/max;
- zoom;
- pan;
- auto-fit;
- reset;
- grid;
- legend;
- line thickness;
- sample markers;
- nearest-sample tooltip;
- synchronized cursor tick.

## 39. Bosch overlays

Independent toggles:

- +0.2 m threshold;
- -0.2 m threshold;
- critical-section shading;
- deadline-miss markers;
- selected Timeline/Event tick.

Default Bosch preset:

```text
Signal: Lateral error
X-axis: seconds
Y-axis: automatic
Overlays:
- thresholds
- critical sections
- deadline misses
```

## 40. Shared selection

Clicking or moving the plot cursor updates the shared selected tick.

Timeline, Canonical Events, and Runtime Inspector may follow existing selection semantics.

Plot interaction must not alter simulation state.

## 41. Data handling

- preserve full-resolution completed-run data;
- use drawing-only downsampling;
- do not change exported values;
- cache plot-series derivation for the completed run;
- do not rebuild series every frame when inputs are unchanged.

---

# Part H — Implementation order

## Phase 1 — Center workspace and Resources

- upper/lower tab-group model;
- right-click movement;
- splitter reuse;
- workspace migration;
- one-table Resources view.

## Phase 2 — Fast execution

- run mode and batch controls;
- lightweight progress;
- cooperative batching;
- complete presentation snapshot publication;
- performance measurements.

## Phase 3 — Completed-run lifecycle and compact Results

- run generation;
- completed-result cache;
- finish-only result build;
- compact summary;
- timing/execution;
- response table;
- Bosch compact summary.

## Phase 4 — Plot Visualizer

- pinned ImPlot;
- visualizer state;
- signal browser;
- lanes/settings;
- completed-run-only data;
- Bosch overlays;
- shared cursor selection.

## Phase 5 — Bosch editability and architecture

- explicit edit policy;
- protected fields;
- baseline/modified status;
- functional dependencies;
- edge legend and selection details.

## Phase 6 — Tests and documentation

Use reviewable commits for each phase within one goal-level Codex conversation.

---

# Part I — Required tests

## 42. Bosch editing

- Bosch project receives `BoschCompatible`;
- task IDs/names cannot change;
- Bosch tasks cannot be added/deleted/duplicated;
- allowed timing/profile/resource edits succeed;
- protected route endpoints remain fixed;
- route delay/send offset can change;
- modified Bosch save/reopen works;
- Bosch runtime reconstructs;
- baseline/modified status is deterministic;
- invalid protected changes cannot Apply;
- pinned Bosch conformance remains unchanged.

## 43. Architecture

- Estimator → Controller exists;
- Controller → Merger exists;
- Feedforward → Merger exists;
- Sensor → Estimator remains a network route;
- Merger → Actuator remains a network route;
- functional dependencies create no message events;
- edge styles and legend are deterministic;
- edge selection reports correct kind.

## 44. Fast mode

- defaults;
- Events batching;
- Ticks batching;
- invalid size rejection;
- unit switching preserves separate values;
- wall-clock guard behavior where testable;
- lightweight progress updates;
- complete presentation snapshot not rebuilt every Fast frame;
- presentation snapshot published on pause/finish/reset/switch-to-Live;
- Live and Fast produce identical:
  - final tick;
  - canonical trace;
  - functional observations;
  - resource totals;
  - Bosch outputs.

## 45. Workspace

- default tab placement;
- right-click movement;
- one-group ownership invariant;
- active-tab repair;
- empty-group collapse;
- reset arrangement;
- old schema migration;
- Fast settings persistence;
- visualizer settings persistence.

## 46. Resources

- utilization fraction;
- zero observed ticks;
- exact label;
- tooltip projection;
- selection mapping;
- no separate subtab state.

## 47. Completed Results

- no `RunResult` build while Running;
- no `RunResult` build while Paused;
- one build when run generation first reaches Finished;
- cache reused across frames;
- invalidation on Reset;
- invalidation on Apply and restart;
- invalidation on project replacement;
- compact summary projection;
- timing/performance values;
- response deadline/miss columns;
- no resource-utilization section;
- Bosch summary available/unavailable behavior.

## 48. Plot Visualizer

- disabled before completion;
- enabled after completed-result cache exists;
- signal search/grouping;
- add/remove/reorder;
- unit-aware lane grouping;
- ticks/seconds conversion;
- full/selected/custom ranges;
- Bosch overlays;
- cursor-to-selection mapping;
- downsampling preserves source data;
- state round trip;
- no partial-run series updates.

## 49. Regressions

Run:

- project tests;
- GUI tests;
- workspace tests;
- controller/session tests;
- analysis/export tests;
- Bosch project tests;
- Bosch conformance;
- canonical-event tests;
- DPI tests;
- CLI tests;
- repository verification targets.

---

# Part J — Acceptance criteria

- Saved Bosch projects are editable within explicit FMU-safe limits.
- Protected Bosch task identities cannot be corrupted.
- Modified Bosch projects save, reopen, and run.
- Missing cloud functional arrows appear without false network semantics.
- Fast mode removes one-event-per-frame coupling.
- Users can choose Events or Ticks and set batch size.
- Progress remains visible and the GUI remains responsive.
- Live and Fast outputs are deterministic and equal.
- Upper/lower center groups remain simultaneously visible.
- Tabs move only through right-click commands.
- Resources has one utilization table.
- Results is built only after the simulation finishes.
- Pausing does not create final Results.
- Results is compact and does not duplicate resource utilization.
- Plot Visualizer is non-modal and completed-run-only.
- ImPlot is used now; Matplot++ is deferred.
- Existing conformance, persistence, export, CLI, DPI, and scheduling behavior remain unchanged.
