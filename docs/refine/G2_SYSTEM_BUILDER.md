# G2 — System Builder

## Goal

Allow users to create and modify a CPSSim system entirely inside the GUI.

Use validated forms and tables first. Do not implement direct Simulink-style canvas editing in this goal.

## Editable model

The builder must support the existing system schema:

- global timing settings;
- preemption mode;
- resources;
- tasks;
- task-resource execution profiles;
- message routes.

The saved result must load through the existing strict configuration loader.

## Builder layout

Use one editor with sections or tabs:

```text
General
Resources
Tasks
Execution Profiles
Message Routes
Validation
```

### General

Fields:

```text
System name
Tick period
Preemption mode
Description (project metadata only)
```

### Resources

Table:

```text
ID | Name
```

Actions:

```text
Add
Duplicate
Remove
```

Initially, every resource keeps the existing exclusive-uniprocessor semantics.

### Tasks

Table:

```text
ID | Name | Period | Deadline | Offset | Priority
```

Actions:

```text
Add
Duplicate
Remove
```

### Execution profiles

Prefer a task-resource matrix:

```text
                 Local CPU    Cloud CPU
Sensor              6            8
Controller           —            9
```

An empty cell means the task cannot execute on that resource.

### Message routes

Table:

```text
Source | Destination | Send offset | Delay
```

Use task names in the UI and stable IDs internally.

## Editing model

Do not mutate the active immutable experiment directly.

Use a separate editable draft:

```text
EditableSystemDraft
        ↓ validate
ExperimentConfig
        ↓ apply
new GuiSimulationSession
```

Applying system changes requires rebuilding the simulation session. Pending edits must not change an active run.

## Validation

Show errors beside the relevant field and in a summary panel.

Required checks include:

- unique positive IDs;
- nonempty names;
- positive periods;
- valid deadlines and offsets;
- valid priorities;
- nonnegative execution times where allowed by the existing model;
- at least one execution profile for every task;
- valid resource/task references;
- no duplicate task-resource profile;
- valid route endpoints;
- route constraints already enforced by the core model.

Reuse existing model/config validation. Do not duplicate canonical validation rules in views.

## Synchronization with architecture view

The architecture view should display either:

- the active system; or
- a validated preview of the builder draft.

Use a clear indicator:

```text
Previewing unapplied system changes
```

Do not allow graph dragging to create/delete model entities in this goal.

## Save behavior

- `Save Project` stores the applied system.
- `Save Draft As...` may be added only if needed.
- Closing with unapplied changes prompts:
  - Apply and save;
  - Discard;
  - Cancel.

## Suggested task split

### G2.1 — Editable draft model

Create graphics-independent draft types and validation tests.

Inspect first:

```text
src/cpssim/model/experiment_config.*
src/cpssim/model/specifications.*
src/cpssim/config/json_config.*
src/cpssim/gui/draft_run_plan.*
```

### G2.2 — Resource and task editors

Acceptance:

- add/edit/remove;
- stable IDs preserved;
- invalid fields are local and do not crash;
- removing referenced entities requires confirmation or is blocked with explanation.

### G2.3 — Execution profile matrix

Acceptance:

- accessible resource cells are editable;
- empty cells remove profiles;
- duplicate profiles cannot be created;
- keyboard navigation is usable.

### G2.4 — Message-route editor

Acceptance:

- source/destination selected by task name;
- invalid/self routes handled according to existing model rules;
- duplicate routes rejected consistently.

### G2.5 — Apply, rebuild, and save

Acceptance:

- valid draft creates a replacement session atomically;
- invalid draft leaves active session unchanged;
- project save/reopen preserves the system.

## Tests

Add tests for:

- draft creation from an existing config;
- add/edit/remove operations;
- ID allocation;
- profile matrix conversion;
- route conversion;
- validation mapping to field diagnostics;
- deterministic JSON round trip;
- atomic session rebuild.

Manual checks:

- create a two-task/two-resource system from scratch;
- save and reopen it;
- apply a run plan and execute it;
- remove a referenced task/resource and verify the warning.

## Non-goals

- custom resource types;
- multicore/global scheduling semantics;
- event-triggered tasks;
- payload ports/channels;
- canvas-based entity creation;
- Bosch-specific model editing.
