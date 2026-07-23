# Step 6 — Final Architecture Consolidation and Acceptance

## Objective

Close the CPSSim interactive Architecture phase without adding another user-facing feature.

This step must:

1. inspect the latest code globally;
2. verify the implemented ownership boundaries;
3. identify confirmed defects and meaningful regression gaps;
4. add only high-value tests;
5. correct only confirmed issues;
6. execute or prepare the final manual acceptance matrix;
7. update implementation and handoff documentation.

Do not add a component library or palette drag/drop.

## 1. Mandatory pre-edit audit

Before changing code, report:

1. exact `main` HEAD SHA and commit message;
2. `git status --short`;
3. registered test list;
4. baseline configure, build, and full test result;
5. structural source of truth;
6. workspace-layout source of truth;
7. structural-selection source of truth;
8. structural Undo/Redo owner;
9. every persistent structural mutation path;
10. structural shortcut ownership;
11. Generic, Bosch-compatible, and read-only policy behavior;
12. running-state restrictions;
13. save, close, Save As, open, and project-replacement behavior;
14. current automated coverage and confirmed gaps.

At minimum inspect:

```text
apps/qt_gui/architecture_view.hpp
apps/qt_gui/architecture_view.cpp
apps/qt_gui/architecture_model.hpp
apps/qt_gui/architecture_model.cpp
apps/qt_gui/structural_edit_controller.hpp
apps/qt_gui/structural_edit_controller.cpp
apps/qt_gui/system_builder_widget.hpp
apps/qt_gui/system_builder_widget.cpp
apps/qt_gui/explorer_widget.hpp
apps/qt_gui/explorer_widget.cpp
apps/qt_gui/main_window.hpp
apps/qt_gui/main_window.cpp
apps/qt_gui/workbench_bridge.hpp
apps/qt_gui/workbench_bridge.cpp
src/cpssim/gui/editable_system_draft.hpp
src/cpssim/gui/editable_system_draft.cpp
src/cpssim/gui/selection_model.hpp
src/cpssim/gui/selection_model.cpp
src/cpssim/gui/system_builder_interaction.hpp
src/cpssim/gui/system_builder_interaction.cpp
src/cpssim/application/project/system_edit_policy.hpp
src/cpssim/application/project/system_edit_policy.cpp
tests/gui/system_builder_interaction_test.cpp
tests/qt_gui/structural_edit_controller_test.cpp
tests/qt_gui/architecture_model_test.cpp
tests/qt_gui/system_builder_widget_test.cpp
tests/qt_gui/main_window_test.cpp
CMakeLists.txt
docs/guide/AGENT-HANDOFF.md
```

Search globally for:

```text
EditableSystemDraft
GuiArchitectureWorkspace
StructuralSelection
QtStructuralEditController
QUndoStack
RestoreDraftCommand
create_task
create_component
duplicate_selected
delete_selected
create_connection
delete_connection
set_message_route
remove_message_route
GuiConnectionKind
GuiConnectionId
DraftMessageRouteKey
notify_structural_selection_changed
draftChanged
applicationStateChanged
workspaceChanged
Qt::Key_Delete
QKeySequence::Delete
WidgetWithChildrenShortcut
apply_and_save_project
restore_draft
replace_project
save_project_as
system_changes_dirty
```

Do not edit until the audit is reported.

## 2. Baseline verification

Run:

```bash
git status --short
git rev-parse HEAD
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest -N --test-dir build/qt-gui
ctest --test-dir build/qt-gui --output-on-failure
```

Record pre-existing failures separately from failures introduced during this step.

Do not claim a passing baseline unless these commands were actually run.

## 3. Ownership audit

### 3.1 Structural truth

Persistent structure must follow:

```text
user command
→ QtStructuralEditController
→ detached before/after state
→ shared QUndoStack
→ QtWorkbenchBridge::restore_draft(...)
→ subscribed view refresh
```

Reject any path in which a QtNodes scene mutation becomes persistent truth.

### 3.2 Layout truth

Verify:

- task positions belong to `GuiArchitectureWorkspace`;
- moving a node does not mutate `EditableSystemDraft`;
- refresh preserves workspace positions;
- project replacement loads the correct workspace;
- placement reuses the established snap and collision-avoidance logic;
- ordinary mouse movement does not perform structural serialization or rebuilds unnecessarily.

### 3.3 Selection truth

Verify shared selection for:

- system;
- resource;
- task;
- execution profile;
- Communication link;
- Logical link;
- deletion fallback;
- Undo/Redo restoration;
- project replacement.

No view may maintain an independent authoritative structural selection.

### 3.4 Undo ownership

Verify:

- exactly one structural `QUndoStack`;
- System Builder, Explorer, and Architecture use the same controller;
- rejected operations add no command;
- no-op operations add no command where no-op detection exists;
- project-root changes do not retain stale commands;
- Undo/Redo restores draft, assignments, and valid selection;
- QtNodes conflicting Undo/Redo/Delete shortcuts remain suppressed;
- CPSSim remains the sole structural Delete owner.

### 3.5 Policy and run state

Verify each mutation for:

- no editable project;
- Generic project;
- Bosch-compatible project;
- read-only adapter project;
- stopped/paused state;
- running state.

Do not duplicate policy logic in individual widgets when the controller already owns the rule.

## 4. Link consistency audit

Verify across domain, draft, controller, Architecture, System Builder, Explorer, persistence, runtime, and tests:

1. links are directed;
2. an ordered task pair has at most one link;
3. reverse direction remains distinct;
4. duplicate creation is rejected;
5. self-loop behavior follows the established domain rule;
6. kinds are Communication and Logical;
7. both kinds persist in Generic projects;
8. Communication rendering is solid;
9. Logical rendering is dashed;
10. Communication latency may be zero or positive;
11. Logical latency is fixed to zero;
12. Logical links generate no network events;
13. both kinds retain the fixed one-tick send offset;
14. Source and Destination display correctly;
15. Source and Destination editing preserve kind and selection;
16. kind conversion preserves endpoints and selection;
17. creation, deletion, conversion, and endpoint changes are undoable;
18. deletion works through:
    - Architecture context menu;
    - Architecture Delete;
    - Experiment Explorer;
19. newly created and loaded links behave identically;
20. refresh produces no duplicate visible link.

The historical loaded-link Delete issue is closed. Reopen it only when current code or a regression test demonstrates a failure.

## 5. Test layers

### Layer A — Graphics-independent interaction

Location:

```text
tests/gui/system_builder_interaction_test.cpp
```

Ensure coverage for:

- task and resource creation;
- explicit-endpoint Communication creation;
- explicit-endpoint Logical creation;
- invalid endpoints;
- duplicate ordered-pair rejection;
- self-loop behavior;
- duplication;
- exact route deletion;
- task deletion and incident-route behavior;
- selection after create, duplicate, conversion, endpoint editing, and delete.

### Layer B — Shared structural edit controller

Location:

```text
tests/qt_gui/structural_edit_controller_test.cpp
```

Ensure coverage for:

- one chronological structural history;
- Undo/Redo;
- rejected/no-op mutations;
- running-state rejection;
- project policy;
- project switching;
- project replacement;
- Save As root behavior;
- mixed commands originating from different GUI paths.

### Layer C — Architecture graph model

Location:

```text
tests/qt_gui/architecture_model_test.cpp
```

Ensure coverage for:

```text
InPortCount = 1
OutPortCount = 1
port index = 0
ConnectionPolicy::Many
```

Also verify:

- isolated tasks;
- fan-in and fan-out;
- task-ID mapping;
- link-ID mapping;
- create callback;
- delete callback;
- duplicate prevention;
- Communication/Logical presentation identity;
- exact rebuild contents;
- no duplicate visible links.

### Layer D — Workbench integration

Locations:

```text
tests/qt_gui/system_builder_widget_test.cpp
tests/qt_gui/main_window_test.cpp
tests/qt_gui/runtime_views_test.cpp
```

Ensure coverage for:

- action state;
- shortcut scope;
- toolbar and context-menu task creation;
- requested-position placement;
- duplicate placement;
- task and link selection synchronization;
- loaded Communication Delete;
- loaded Logical Delete;
- Source/Destination/kind editing;
- main-window Undo/Redo actions;
- dirty state;
- pending editor commit before Save;
- save/reload;
- project open/replacement;
- close confirmation;
- immediate appearance refresh.

## 6. Mixed chronological Undo/Redo

Create or complete one representative test sequence:

1. add task;
2. edit a task property;
3. duplicate a task;
4. create a Communication link;
5. convert it to Logical;
6. change an endpoint where valid;
7. delete the link.

Undo each operation in reverse order and verify exact state after every step.

Redo each operation and verify exact state again.

The test must use the shared controller and existing main-window actions where appropriate. Do not create a test-only undo mechanism.

## 7. Save and reload

For a Generic project, test automatically where practical:

1. add a task;
2. edit task properties;
3. add or edit a resource;
4. assign the task and set an execution profile;
5. move the task node;
6. create a Communication link;
7. create a Logical link on a distinct ordered pair;
8. save;
9. close or replace the project;
10. reopen.

Verify:

- task and resource state;
- assignments and execution profiles;
- link endpoints and kinds;
- Communication latency, including zero;
- Logical latency zero;
- fixed one-tick send offset;
- task-node positions;
- valid selection or safe fallback;
- graph node/link count;
- clean dirty state after load.

## 8. Static review checklist

Inspect for:

- duplicate structural Undo stacks;
- direct `EditableSystemDraft` mutation in Architecture view/model;
- scene mutation used as persistent truth;
- duplicate authoritative selection state;
- commented-out implementation;
- unrelated formatting;
- dangling lambda captures;
- controller dependencies on QWidget dialogs or QtNodes;
- unrestricted application-level Delete;
- active QtNodes conflicting structural shortcuts;
- graphics-adapter hardcoding of simulation timing semantics;
- missing CMake source/test registration;
- stale documentation describing the graph as structurally read-only;
- stale component-library or palette drag/drop requirements.

## 9. Manual acceptance matrix

Use `PASS`, `FAIL`, `N/E` (not executed), or `N/A`.

### 9.1 Generic project — stopped or paused

#### Task and resource operations

- [ ] Add Task from Architecture toolbar
- [ ] Add Task from empty-canvas context menu
- [ ] Add occurs near the intended position
- [ ] Repeated additions avoid direct overlap
- [ ] Duplicate selected task
- [ ] Edit task properties
- [ ] Edit assignment and execution profile
- [ ] Execution-profile edit preserves assignment
- [ ] Delete task using context menu
- [ ] Delete task using Delete
- [ ] Add Resource through Experiment Explorer
- [ ] Edit Resource
- [ ] Confirm no Resource node appears in Architecture
- [ ] Undo/Redo task and resource operations

#### Link operations

For both Communication and Logical:

- [ ] Create by output-port to input-port drag
- [ ] Verify direction
- [ ] Verify duplicate ordered-pair rejection
- [ ] Verify rendering style
- [ ] Select in Architecture
- [ ] Select in Explorer
- [ ] Inspect correct Source and Destination
- [ ] Edit Source
- [ ] Edit Destination
- [ ] Convert kind
- [ ] Verify zero-latency behavior
- [ ] Delete using context menu
- [ ] Delete using Delete
- [ ] Delete using Explorer
- [ ] Undo/Redo creation
- [ ] Undo/Redo deletion
- [ ] Undo/Redo conversion
- [ ] Undo/Redo endpoint editing

Include one loaded Communication link and one loaded Logical link in Delete testing.

#### Selection synchronization

- [ ] Explorer task → Architecture
- [ ] Explorer task → System Builder
- [ ] Architecture task → Explorer
- [ ] Architecture task → System Builder
- [ ] Explorer link → Architecture
- [ ] Explorer link → System Builder
- [ ] Architecture link → Explorer
- [ ] Architecture link → System Builder
- [ ] Resource selection clears graph selection appropriately
- [ ] Delete creates valid fallback selection
- [ ] Undo/Redo restores valid selection

#### Save and lifecycle

- [ ] Real structural edit marks dirty
- [ ] No-op interaction does not mark dirty
- [ ] Save commits a pending editor value
- [ ] Successful Save clears dirty
- [ ] Reopen preserves tasks, resources, assignments, profiles, links, and positions
- [ ] Close: Apply and Save
- [ ] Close: Discard
- [ ] Close: Cancel
- [ ] Project replacement
- [ ] Save As
- [ ] Undo history does not cross project roots

### 9.2 Generic project — running

- [ ] Task creation rejected
- [ ] Task deletion rejected
- [ ] Link creation rejected
- [ ] Link deletion rejected
- [ ] Link conversion rejected
- [ ] Endpoint editing rejected
- [ ] Rejected action adds no undo entry
- [ ] Selection still works
- [ ] Pan and zoom still work
- [ ] Fit and 100% still work
- [ ] Runtime highlighting still works
- [ ] Editing resumes correctly after pause/reset

### 9.3 Bosch-compatible project

- [ ] Protected task creation rejected
- [ ] Protected task duplication rejected
- [ ] Protected task deletion rejected
- [ ] Protected link creation rejected
- [ ] Protected link deletion rejected
- [ ] Protected link conversion rejected
- [ ] Protected endpoint editing rejected
- [ ] Protected entities remain selectable
- [ ] Protected entities remain inspectable
- [ ] Diagnostic/help text is clear
- [ ] Hidden adapter handoff remains hidden
- [ ] Simulation semantics remain unchanged

### 9.4 Layout and appearance

- [ ] Dark theme
- [ ] Light theme
- [ ] Canvas updates immediately after theme change
- [ ] Task boxes remain legible
- [ ] Communication/Logical styles remain distinguishable
- [ ] Grid remains visually consistent
- [ ] Architecture and dock boundaries remain visible
- [ ] Narrow System Builder remains usable
- [ ] Wide System Builder remains usable
- [ ] Fit works
- [ ] 100% works
- [ ] Auto Layout works
- [ ] Snap to Grid works
- [ ] Saved workbench layout restores

### 9.5 DPI transition

When suitable hardware is available:

- [ ] Move CPSSim between monitors with different DPI
- [ ] No font-size/rasterizer assertion
- [ ] No crash
- [ ] Canvas repaints immediately
- [ ] Hit testing remains aligned
- [ ] Theme switch still works
- [ ] Node movement still works

Mark unavailable hardware as `N/E`. Do not claim it passed.

## 10. Performance sanity

Use a nontrivial project and check:

- context menu opens promptly;
- selection does not recurse or visibly flicker;
- node dragging remains smooth;
- connection dragging remains smooth;
- refresh creates no duplicate links;
- repeated add/undo/redo does not show obvious unbounded growth;
- Save remains responsive;
- console does not produce repeated unexplained warnings.

Do not perform speculative optimization without evidence.

## 11. Documentation updates

Update documentation to match current code.

At minimum review:

```text
docs/guide/AGENT-HANDOFF.md
docs/refine/CPSSim_Architecture_Interaction_Plan/00_README.md
docs/refine/CPSSim_Architecture_Interaction_Plan/06_FINAL_CONSOLIDATION_AND_ACCEPTANCE.md
docs/refine/CPSSim_Architecture_Interaction_Plan/07_COPILOT_PROMPTS.md
```

Document:

- completed interaction capabilities;
- exact ownership boundaries;
- current link model and runtime semantics;
- test commands;
- known limitations;
- removed component-library and palette drag/drop scope;
- recommended transition from GUI embellishment to simulator and experiment capabilities.

Do not rewrite Steps 1–5 unless a factual correction is necessary.

## 12. Prohibited changes

The coding agent must not:

- add a component library;
- add palette MIME drag/drop;
- redesign the workbench;
- redesign Experiment Explorer;
- introduce another controller;
- introduce another structural `QUndoStack`;
- make QtNodes persistent structural truth;
- add resource nodes to Architecture;
- create endpoint-less links;
- change scheduling semantics;
- change FMU semantics;
- change event ordering;
- change the fixed one-tick send offset;
- expose hidden Bosch adapter semantics;
- add new link kinds;
- add unrelated formatting;
- commit automatically.

Only correct issues confirmed by code, a failing test, or a reproducible scenario.

## 13. Final build

Run:

```bash
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest --test-dir build/qt-gui --output-on-failure
```

## 14. Completion gate

The Architecture interaction phase is complete only when:

```text
all registered tests pass
+ ownership/static audit passes
+ all applicable critical manual checks pass
+ skipped checks are explicit
+ no unexplained warnings or errors remain
+ documentation matches current code
```

## 15. Final report

Report:

1. exact HEAD inspected;
2. baseline build/test result;
3. confirmed issues;
4. files changed and rationale;
5. tests added or changed;
6. final build/test result;
7. ownership invariants verified;
8. link invariants verified;
9. manual acceptance result;
10. checks not executed;
11. documentation updated;
12. known limitations;
13. remaining risks;
14. concise diff summary.

Do not begin new simulator functionality in this step.
