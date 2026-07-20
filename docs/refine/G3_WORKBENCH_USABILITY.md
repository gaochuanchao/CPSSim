# G3 — Workbench Usability

## Goal

Improve readability and navigation without changing simulation semantics.

This goal combines:

- theme selection;
- adjustable panel heights;
- Resources layout;
- Canonical Events table;
- Inspector wording and field layout;
- workspace persistence.

## G3.1 — Light and dark themes

Add:

```text
View → Theme → Light
View → Theme → Dark
```

Optional later:

```text
Follow system
```

Requirements:

- theme is presentation-only;
- store it in `workspace.json`;
- update the OpenGL clear color consistently;
- preserve current DPI behavior;
- rebuild an unscaled base style when theme changes, then apply current display scale;
- never repeatedly scale an already scaled style.

Inspect first:

```text
apps/gui/main.cpp
apps/gui/gui_application.*
src/cpssim/gui/display_scale.*
```

## G3.2 — Adjustable vertical layout

Replace fixed height ratios with draggable horizontal splitters:

```text
Analysis views
================ splitter
Resources
================ splitter
Canonical Events
```

Store normalized ratios:

```text
analysis_lower_split
resource_event_split
```

Requirements:

- sensible minimum heights;
- no panel becomes permanently unreachable;
- ratios survive resize and DPI changes;
- ratios are saved in workspace state;
- hiding one panel redistributes space cleanly.

Inspect first:

```text
apps/gui/gui_application.cpp
```

Do not introduce full docking in this task.

## G3.3 — Resources view

Current problem: the utilization chart is frequently clipped.

Recommended layout:

```text
[Resource State] [Utilization]
```

### Resource State table

Keep:

```text
Resource | Running | Ready | Busy ticks | Idle ticks
```

Add when already derivable:

```text
Utilization
```

### Utilization tab

Use labeled bars, not an unlabeled histogram.

Each bar must identify its resource. Selecting a bar selects the resource globally.

The plot must use detached snapshot data only.

Inspect first:

```text
apps/gui/views/resource_view.*
src/cpssim/gui/presentation_model.*
```

## G3.4 — Canonical Events table

Replace raw JSON rows with a virtualized ImGui table.

Columns:

```text
Sequence
Tick
Time
Type
Phase
Task
Job
Resource
Message
Vehicle
Cause
```

Requirements:

- default order remains canonical sequence order;
- optional columns may be hidden;
- horizontal scrolling allowed;
- use an ImGui list clipper or equivalent;
- selecting a row updates global event and tick selection;
- clicking `Cause` selects the predecessor event;
- full raw JSON remains available in a details popup or Inspector;
- no change to `Event` or JSON serialization.

Filters:

```text
Type
Task
Resource
Vehicle
Text/search
```

Filtering and sorting are presentation-only.

Inspect first:

```text
apps/gui/views/event_view.*
src/cpssim/model/event.*
src/cpssim/model/categories.*
src/cpssim/trace/event_json.*
src/cpssim/gui/selection_model.*
```

## G3.5 — Inspector terminology and property layout

Replace repeated internal “Draft” wording.

Preferred labels:

```text
Run Configuration
Status: Unapplied changes

Resource
Execution time
Stop tick
Currently applied
Validate changes
Apply and restart
```

Use a two-column property grid:

```text
Label              Control/value
```

Requirements:

- fixed or bounded label column;
- controls use remaining width;
- numeric controls do not expand unnecessarily;
- long text shows full value in a tooltip;
- path fields use `[field] [Browse...]`;
- combo preview text remains visible;
- Inspector remains usable at minimum supported width.

Inspect first:

```text
apps/gui/views/run_plan_editor.*
apps/gui/views/inspector_view.*
```

## G3.6 — Workspace persistence

Persist only presentation state:

- theme;
- visible panels;
- split ratios;
- active analysis tab;
- selected signal IDs;
- plot viewport where practical;
- event filters.

Do not persist:

- active runtime objects;
- event traces;
- draft run-plan changes;
- simulation current tick;
- hidden semantic state.

Unknown workspace fields should be ignored or rejected according to the selected strict versioning policy. Document the policy.

## Suggested implementation order

```text
G3.5 Inspector wording/layout
G3.2 Splitters
G3.3 Resources layout
G3.4 Event table
G3.1 Themes
G3.6 Persistence
```

This order gives visible improvements early and postpones persistence until the state model is stable.

## Tests

Headless/model tests:

- splitter ratio clamping;
- event-row projection;
- filter predicates;
- cause navigation;
- workspace JSON round trip;
- theme enum persistence;
- presentation state does not alter run-plan signature.

Manual checks:

- narrow and wide windows;
- high-DPI monitor movement;
- light/dark switch;
- hide/show every panel;
- large event trace scrolling;
- long resource/task/path names;
- resource plot fully visible.

## Non-goals

- docking;
- multiple independent windows;
- changing event schemas;
- changing resource semantics;
- exporting data;
- scenario-specific result plots.
