# Extended Goal 2 — Explorer-Driven System Builder

## Goal

Use the Experiment Explorer as the main interface for creating, selecting,
duplicating, and deleting system entities. Use a System Builder panel directly
below the Explorer to edit the currently selected structural object.

Reserve the right side for run configuration and runtime inspection.

## Target layout

```text
+----------------------+-----------------------------+----------------------+
| Experiment Explorer  |                             | Run Configuration    |
|                      |       Main Workbench        |                      |
| System               |                             | Policy               |
| ├─ Resources         | Architecture / Timeline /   | Stop tick            |
| ├─ Tasks             | Signals / Results           | Task assignments     |
| ├─ Profiles          |                             | Validate / Apply     |
| └─ Routes            |                             |                      |
+----------------------+                             +----------------------+
| System Builder       |                             | Runtime Inspector    |
| Selected structural  |                             | Selected event/job/  |
| object properties    |                             | runtime state        |
+----------------------+-----------------------------+----------------------+
```

The left and right sidebars each use a draggable horizontal splitter. Goal 2
implements the splitters and in-memory state. Goal 3 may persist their positions.

## Panel responsibilities

### Experiment Explorer

The Explorer owns structural navigation and lifecycle actions:

- create;
- select;
- duplicate;
- delete;
- expand/collapse.

Suggested tree:

```text
System
├── Resources
│   ├── Local CPU
│   └── Cloud CPU
├── Tasks
│   ├── Sensor
│   └── Controller
├── Execution Profiles
│   ├── Sensor → Local CPU
│   └── Controller → Cloud CPU
└── Message Routes
    └── Sensor → Controller
```

### System Builder

The System Builder edits the current Explorer selection.

| Explorer selection | System Builder content |
|---|---|
| System | Tick period and preemption mode |
| Resources header | Resource overview |
| Resource | Resource name and ID |
| Tasks header | Task overview |
| Task | Name, period, deadline, offset, priority |
| Profiles header | Task-resource matrix |
| Profile | Task, resource, execution time |
| Routes header | Route table |
| Route | Source, destination, send offset, delay |

### Run Configuration

Contains run-plan settings only:

- scheduling policy;
- stop tick;
- task-to-resource assignments;
- applied/pending state;
- Validate changes;
- Apply and restart.

### Runtime Inspector

Contains runtime observations only:

- selected event;
- selected job;
- selected runtime resource state;
- selected timeline interval;
- raw event JSON;
- concise empty-selection guidance.

Structural system properties must not be duplicated in Runtime Inspector.

## Selection model

Use two explicit selection domains.

### Structural selection

Produced by the Explorer:

- System;
- section header;
- ResourceId;
- TaskId;
- profile key;
- route key.

Structural selection controls the System Builder.

### Runtime selection

Produced by Timeline, Canonical Events, resource views, or signal views.

Runtime selection controls the Runtime Inspector.

Shared highlighting is allowed, but the two selections must not overwrite each
other. Use stable IDs or composite keys, never display names as identity.

## Editable system draft

All structural edits modify an `EditableSystemDraft`.

```text
Explorer action or property edit
        ↓
EditableSystemDraft
        ↓
Validate changes
        ↓
Canonical ExperimentConfig
        ↓
Construct replacement session
        ↓
Apply and restart atomically
```

The draft must represent:

- tick period;
- preemption mode;
- resources;
- tasks;
- deterministic task-resource profiles;
- message routes.

It must be GUI-independent and headless-testable.

## Explorer context menus

Section headers:

```text
Resources          → Add Resource
Tasks              → Add Task
Execution Profiles → Add Execution Profile
Message Routes     → Add Message Route
```

Entity menus:

```text
Duplicate <entity>
Delete <entity>...
```

An explicit Edit command is unnecessary because selection opens the editor.

## Create-and-focus behavior

After creating an entity:

1. add it to the draft;
2. allocate stable identity;
3. assign valid defaults;
4. expand its parent section;
5. select it;
6. scroll it into view;
7. show its editor;
8. focus the primary field, normally Name.

Suggested defaults:

```text
Resource:
  Name = New Resource
  ID = next unused resource ID

Task:
  Name = New Task
  ID = next unused task ID
  Period = 100
  Deadline = 100
  Offset = 0
  Priority = 1
```

For profiles and routes, choose the first valid unused combination. Disable the
Add action with an explanation when no valid new object can be created.

## Deletion

Use explicit confirmed cascade deletion.

Example:

```text
Delete resource "Cloud CPU"?

This will also remove:
- 3 execution profiles
- 2 draft run-plan assignments

[Cancel] [Delete Resource]
```

Example:

```text
Delete task "Controller"?

This will also remove:
- 2 execution profiles
- 1 incoming route
- 1 outgoing route
- 1 draft run-plan assignment

[Cancel] [Delete Task]
```

Rules:

1. Compute dependent objects before opening the dialog.
2. Show affected counts.
3. Cancel changes nothing.
4. Confirmation changes drafts only.
5. The active applied system remains unchanged.
6. After deletion, select the nearest sibling or the parent section.
7. Mark the system draft modified.

## Duplicate behavior

- Allocate a new stable identity.
- Use a distinct default name for tasks/resources.
- Do not automatically duplicate all dependent profiles/routes.
- Prevent invalid identical profile/route duplication.

## Sidebar layout

Left:

```text
Experiment Explorer
==================== draggable splitter
System Builder
```

Right:

```text
Run Configuration
================== draggable splitter
Runtime Inspector
```

Requirements:

- minimum heights;
- scrolling where needed;
- in-memory normalized ratios;
- no full docking;
- preserve DPI behavior.

## Runtime Inspector content

Selected event:

```text
Sequence
Tick
Physical time
Type
Phase
Task
Job
Resource
Message
Vehicle
Cause
Raw JSON
```

Selected job:

```text
Task
Job ID
Lifecycle
Release tick
Start tick
Finish tick
Deadline
Response time
Assigned resource
```

Selected runtime resource:

```text
Running job
Ready jobs
Busy ticks
Idle ticks
Utilization
```

No runtime selection:

```text
No runtime item selected.
Select an event, job, timeline item, or resource state.
```

Goal 4 owns full results analysis.

## Editing restrictions

- Structural editing is disabled while the simulation is actively running.
- Explain that the user must pause/reset first.
- Draft edits do not change runtime state.
- Apply and restart rebuilds the session atomically.
- Invalid drafts cannot replace the active session.
- Save Project writes the applied system.

## Architecture constraints

- Simulator core must not depend on Dear ImGui.
- Explorer rendering must not contain validation rules.
- Cascade analysis belongs in draft/application logic.
- Runtime Inspector reads detached snapshots only.
- Bosch-specific construction remains outside generic system editing.
- No direct graph editing.
- No new simulator semantics.

## Implementation phases

1. Draft model, diagnostics, stable IDs, cascade analysis, and separate selection models.
2. Left/right split layout and panel responsibility migration.
3. Explorer context menus and create/select/focus/delete behavior.
4. System Builder editors, profile matrix, and route table.
5. Validate, Apply and restart, prompts, save/reopen.
6. Focused tests and documentation.

Use reviewable commits for these phases in one Goal 2 implementation context.

## Required tests

- config → draft → config;
- minimal new draft;
- stable ID allocation;
- dirty tracking;
- add/duplicate/delete;
- cascade impact and execution;
- profile/route uniqueness;
- creation selects and focuses the new entity;
- deletion repairs structural selection;
- structural and runtime selections remain independent;
- disabled Add conditions;
- confirmation cancel/confirm;
- active system unchanged before Apply;
- atomic successful rebuild;
- invalid draft/run plan;
- Save/reopen;
- close/open prompt decisions;
- GUI, DPI, canonical ordering, Bosch conformance, CLI, and Make regressions.

## Acceptance criteria

- Explorer manages structural entity lifecycle.
- New entities become visible, selected, and editable below Explorer.
- Entity deletion is dependency-aware and confirmed.
- System Builder edits structural definitions.
- Run Configuration remains upper right.
- Runtime Inspector occupies lower right.
- Structural data is not duplicated on the right.
- Draft changes require Apply and restart.
- A supported system can be created, applied, saved, and reopened.
- No Goal 3 persistence or Goal 4 results/export work is added beyond required in-memory layout state.
