# Step 8 — Automated Tests and Manual Acceptance Matrix

## Objective

Provide a completion gate for the interactive Architecture editor.

A feature is not complete because it appears to work once. It must preserve:

- domain consistency;
- undo/redo;
- selection synchronization;
- project policy;
- run-state protection;
- layout persistence;
- save/reload behavior;
- high-DPI and theme behavior.

## 1. Test layers

Use four layers.

### Layer A — Graphics-independent domain interaction

Location:

```text
tests/gui/system_builder_interaction_test.cpp
```

Covers:

- create task/resource/route;
- duplicate;
- delete;
- validation;
- endpoint and duplicate-route rules;
- selection result.

No Qt widgets or scene objects.

### Layer B — Shared Qt edit controller

Suggested location:

```text
tests/qt_gui/structural_edit_controller_test.cpp
```

Covers:

- undo/redo;
- active project switching;
- run-state checks;
- edit policy;
- bridge notification behavior;
- mixed command history.

### Layer C — Architecture graph model

Location:

```text
tests/qt_gui/architecture_model_test.cpp
```

Covers:

- stable node/port/connection mapping;
- fixed ports;
- callback invocation;
- no local duplication;
- connection create/delete request mapping.

### Layer D — Qt workbench integration

Suggested locations:

```text
tests/qt_gui/architecture_view_test.cpp
tests/qt_gui/main_window_test.cpp
tests/qt_gui/runtime_views_test.cpp
```

Covers:

- QAction state;
- selection synchronization;
- dock raising;
- add-at-position;
- add-at-center;
- component library;
- drag/drop handler;
- global workbench undo/redo.

## 2. Required domain tests

### Task creation

- creates exactly one task;
- creates any required assignment placeholder;
- selects new task;
- preserves existing tasks;
- returns diagnostic on failure.

### Duplicate task

- creates exactly one distinct ID;
- copies intended editable properties;
- does not duplicate protected identity incorrectly;
- creates/updates assignment according to current semantics;
- selects duplicate.

### Delete task

- removes task;
- removes/updates assignments;
- removes incident routes according to existing semantics;
- produces valid fallback selection;
- is fully undoable through controller snapshot.

### Connection creation

- valid source/destination;
- missing source;
- missing destination;
- duplicate pair;
- self-loop according to policy;
- default route fields match existing domain conventions;
- new route selected.

### Connection deletion

- deletes exact route;
- does not delete unrelated route;
- selection remains valid.

## 3. Required controller tests

### Chronological mixed history

Perform:

1. edit task period;
2. add task from Architecture path;
3. duplicate from context command;
4. create connection;
5. delete connection.

Then undo five times and verify exact state after each undo.

Redo five times and verify exact state after each redo.

### Project switch

- command in project A;
- switch to B;
- old undo unavailable;
- switch back to A;
- do not resurrect stale command objects.

### Save As

Verify controller root synchronization behavior follows application semantics.

### No-op/failure

A rejected command must not add an undo entry.

### Running state

All structural mutations reject.

### Protected policy

Protected operations reject without corrupting status or selection.

## 4. Required graph-model tests

### Node ports

Every task:

```text
InPortCount = 1
OutPortCount = 1
```

### Existing routes

All route connections use port `0`.

### Fan-in/fan-out

Many connections may share a port.

### Callback mapping

Connection create request receives correct task IDs.

### Duplicate prevention

A→B existing means a second A→B is rejected.

### Delete mapping

Selected visual connection maps to correct `GuiConnectionId`.

### Rebuild

After domain update:

- old cache clears;
- new cache contains exact nodes/connections;
- no duplicate visible edge.

## 5. Required view tests

### Action state table

| State | Add | Duplicate | Delete | Edit | Fit |
|---|---:|---:|---:|---:|---:|
| No project | Off | Off | Off | Off | On |
| Generic paused, no selection | On | Off | Off | Off | On |
| Generic paused, task selected | On | On | On | On | On |
| Generic paused, connection selected | On | Off | On | On | On |
| Generic running | Off | Off | Off | Selection-dependent | On |
| Bosch/protected | Off for protected edits | Off | Off | On | On |

Adapt resource-specific policy after auditing actual rules.

### Add at cursor

Verify requested scene position and snap/collision handling.

### Duplicate placement

Verify new node does not overlap original.

### Delete shortcut scope

- canvas focused: Delete invokes graph command;
- System Builder line edit focused: Delete edits text only.

### Edit action

Raises System Builder dock and preserves selection.

## 6. Required component-library tests

- library exists;
- Task action emits one taskCreated;
- Resource creates resource only;
- Connection action enters/instructs connect mode;
- actions track run state and policy;
- splitter remains valid after resize;
- library survives page changes.

## 7. Required drag/drop tests

- MIME type exact;
- task accepted;
- resource rejected;
- connection rejected;
- drop coordinate transformed;
- one task created;
- undo works;
- no task created during dragMove.

## 8. Save/reload tests

For a generic project:

1. create task;
2. move task;
3. create connection;
4. edit task;
5. save;
6. close;
7. reopen.

Verify:

- task exists;
- route exists;
- task properties exist;
- node position exists;
- selection is valid or safely reset;
- graph model count matches domain count.

## 9. Build commands

Run:

```bash
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest --test-dir build/qt-gui --output-on-failure
```

List tests when debugging:

```bash
ctest --test-dir build/qt-gui -N
```

Run a focused test by regex:

```bash
ctest --test-dir build/qt-gui -R architecture --output-on-failure
```

Use actual registered names.

## 10. Static review checklist

Inspect the final diff for:

- duplicate undo stacks;
- direct `EditableSystemDraft` mutation in Architecture model;
- direct scene mutation used as domain truth;
- commented-out code;
- unrelated formatting;
- raw owning pointers without QObject parent or clear owner;
- lambda captures of local references that outlive scope;
- dialogs inside controller/model;
- global Delete shortcut;
- missing object names for tested actions/widgets;
- hardcoded connection timing defaults in the Qt adapter;
- missing CMake source/test registration.

## 11. Manual acceptance matrix

### Generic project — stopped

- [ ] Add Task from canvas context menu
- [ ] Add Task from toolbar
- [ ] Add Task from component library
- [ ] Drag Task from library to canvas
- [ ] Duplicate selected task
- [ ] Delete selected task
- [ ] Edit selected task in System Builder
- [ ] Create connection by port drag
- [ ] Select connection
- [ ] Edit connection
- [ ] Delete connection
- [ ] Undo/redo each operation
- [ ] Mixed undo/redo order correct
- [ ] Save/reload correct

### Generic project — running

- [ ] Structural actions disabled
- [ ] Drag/drop rejected
- [ ] Port connection rejected
- [ ] Selection still works
- [ ] Pan/zoom still works
- [ ] Runtime highlighting still works

### Bosch/protected project

- [ ] Protected task creation rejected
- [ ] Protected duplication rejected
- [ ] Protected deletion rejected
- [ ] Protected route creation/deletion rejected
- [ ] Selection and inspection work
- [ ] No hidden adapter semantics exposed or changed

### Layout and appearance

- [ ] Dark theme
- [ ] Light theme
- [ ] Narrow System Builder
- [ ] Wide System Builder
- [ ] Architecture tab boundary preserved
- [ ] Dock boundaries preserved
- [ ] 100%/Fit/Auto Layout work
- [ ] Snap to Grid works
- [ ] high-DPI monitor change
- [ ] no crash after DPI change
- [ ] toolbar/action icons or text remain legible

### Selection synchronization

- [ ] Explorer → Architecture
- [ ] Architecture → Explorer
- [ ] Architecture → System Builder
- [ ] Resource Assignment highlight → Architecture
- [ ] connection selection → System Builder
- [ ] delete → valid fallback selection

## 12. Performance sanity

Use a project with many tasks/routes.

Check:

- context menu opens immediately;
- selection does not rebuild recursively;
- dragging a node remains smooth;
- connection drag remains smooth;
- refresh does not create duplicate scene objects;
- no repeated full project serialization per mouse move;
- memory does not grow after repeated add/undo cycles.

## 13. Completion gate

Do not call the feature complete until:

```text
all ctest tests pass
+ all static review items pass
+ all applicable manual matrix items pass
+ no unexplained warnings/errors in console
+ documentation updated
```
