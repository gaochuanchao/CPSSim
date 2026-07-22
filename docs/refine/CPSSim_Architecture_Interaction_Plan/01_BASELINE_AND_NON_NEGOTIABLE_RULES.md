# Step 1 — Baseline Audit and Non-Negotiable Rules

## Objective

Establish a clean baseline and make Copilot understand the existing architecture before it changes any code.

This is not optional. Most mistakes in an interactive graph editor come from modifying the graphics layer without understanding the domain-edit path, undo ownership, selection ownership, or project edit policy.

## 1. Baseline verification

Run:

```bash
git status --short
git rev-parse HEAD
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest --test-dir build/qt-gui --output-on-failure
```

Expected baseline commit when this plan was written:

```text
c396614a417caa706c3ef9903a40c01561c52b1d
```

A different commit is acceptable, but Copilot must re-read the relevant symbols.

Do not begin feature development when:

- the worktree contains unrelated changes;
- the Qt GUI does not build;
- an existing Qt test fails;
- the Architecture tab cannot render;
- node selection no longer updates System Builder;
- node movement or layout persistence is already broken.

## 2. Required code audit

Before editing, Copilot must inspect and summarize these symbols.

### Architecture view

```text
QtArchitectureView::QtArchitectureView
QtArchitectureView::refresh
QtArchitectureView::select_node
QtArchitectureView::select_scene_item
QtArchitectureView::persist_node_position
QtArchitectureView::add_task_at
QtArchitectureView::place_task_near_view_center
QtArchitectureView::snap_node_position
QtArchitectureView::auto_layout
QtArchitectureView::synchronize_scene_selection
QtArchitectureGraphicsView::drawBackground
```

### Architecture graph model

```text
QtArchitectureGraphModel::rebuild
QtArchitectureGraphModel::nodeData
QtArchitectureGraphModel::setNodeData
QtArchitectureGraphModel::portData
QtArchitectureGraphModel::connectionPossible
QtArchitectureGraphModel::addConnection
QtArchitectureGraphModel::deleteConnection
QtArchitectureGraphModel::deleteNode
QtArchitectureGraphModel::connection_for
QtArchitectureGraphModel::entity_for
QtArchitectureGraphModel::node_id_for
```

### Structural editing

```text
QtSystemBuilderWidget::push_mutation
QtSystemBuilderWidget::create_component
QtSystemBuilderWidget::duplicate_selected
QtSystemBuilderWidget::delete_selected
RestoreDraftCommand
DraftState
SystemExplorerInteraction::create
SystemExplorerInteraction::duplicate
SystemExplorerInteraction::confirm_delete
QtWorkbenchBridge::restore_draft
```

### Main-window ownership and wiring

```text
QtMainWindow::bind_workbench
undo_action_
redo_action_
system_builder_
QtArchitectureView construction
QtSystemBuilderWidget construction
taskCreated connection
```

### Project policy and run-state checks

```text
ProjectSystemEditPolicy
project_system_edit_policy(...)
GuiRunState::Running
WorkbenchApplication::editable_system()
WorkbenchApplication::structural_selection()
```

Copilot must explain where each of the following currently lives:

- domain truth;
- scene/model cache;
- selection;
- undo stack;
- graph layout positions;
- edit policy;
- draft validation;
- bridge notifications.

## 3. Sources of truth

The implementation must preserve these ownership boundaries.

### Structural truth

Structural truth belongs to:

```text
EditableSystemDraft
run assignments
StructuralSelection
```

The exact state bundle currently used by undo commands should remain the basis of structural undo/redo.

### Graphics truth

The QtNodes graph model may cache:

```text
node ID mappings
node captions
node sizes
node positions
port counts
connection IDs
presentation decorations
```

This cache must be reconstructible from the application state.

### Workspace layout truth

Task-node positions belong to:

```text
GuiArchitectureWorkspace
```

Moving a node is a workspace/layout edit, not a structural model edit.

Do not put node positions inside `EditableSystemDraft`.

### Selection truth

Selection belongs to the application's shared `StructuralSelection`.

Do not create an independent Architecture-only selected-task variable as the authoritative selection.

## 4. Non-negotiable rules

### Rule 1 — No direct scene mutation as domain mutation

Forbidden:

```cpp
nodes_.erase(...);
connections_.push_back(...);
scene_->createNode(...);
scene_->deleteConnection(...);
```

when the intention is to edit the CPSSim system.

The domain must be changed first. The scene then rebuilds from the domain.

### Rule 2 — One structural undo stack

There must be exactly one `QUndoStack` for structural edits in the active Qt workbench.

Forbidden:

- one stack in System Builder and another in Architecture;
- scene movement commands mixed into the structural stack without an explicit design decision;
- bypassing undo for canvas-created tasks while System Builder uses undo.

### Rule 3 — Preserve adapter-owned protection

For Bosch or other adapter-owned project types:

- task identities and route structure remain protected;
- actions must be disabled or rejected consistently;
- do not enable editing by changing `ProjectSystemEditPolicy`;
- do not add special backdoors in Architecture.

### Rule 4 — Disable structural edits while running

When:

```cpp
application.run_state() == GuiRunState::Running
```

all structural edit actions must be disabled or reject the request cleanly.

Node selection, viewing, panning, zooming, and runtime highlighting may remain available.

### Rule 5 — No simulation-kernel changes

This GUI task must not modify:

```text
scheduler semantics
event semantics
FMU execution
runtime engine
task release/deadline behavior
message timing semantics
```

A request to create a connection may require an existing domain-layer interaction API. Extend that narrow interaction API when needed; do not change the simulation model merely to simplify the GUI.

### Rule 6 — No duplicate domain model

Do not introduce a new GUI-owned list of tasks or routes.

The graph model should remain an adapter over existing presentation/domain state.

### Rule 7 — Do not rebuild the entire application on mouse movement

Node dragging may update workspace position, but it must not trigger structural reconstruction on every mouse-move event.

Persist or notify at the existing node-moved granularity. Preserve current behavior unless profiling proves it problematic.

### Rule 8 — No global Delete shortcut that deletes while editing text

Delete must operate on graph selection only when the Architecture view or its graphics view has focus.

Use an appropriate shortcut context such as:

```cpp
Qt::WidgetWithChildrenShortcut
```

Do not make Delete an unrestricted application shortcut.

### Rule 9 — Reuse QActions

One user command should have one `QAction`, reused by:

- context menus;
- Architecture toolbar;
- optional Edit menu;
- keyboard shortcut.

Do not implement separate lambdas with different policy checks for the same command.

### Rule 10 — No commented-out old implementation

After each migration:

- delete old code;
- do not leave commented-out `setWidget`, mutation, or signal blocks;
- do not leave speculative TODOs unless explicitly required by this plan.

## 5. Required baseline tests

Before Step 2, verify manually:

1. Open a generic project.
2. Select a task in Experiment Explorer.
3. Confirm Architecture selects the same node.
4. Select a node in Architecture.
5. Confirm System Builder shows that task.
6. Move a node.
7. Close and reopen the project/workbench.
8. Confirm position persistence.
9. Toggle Snap to Grid.
10. Switch dark/light theme.
11. Confirm selection and graph remain functional.
12. Open a Bosch project and confirm protected fields remain protected.

Record the baseline behavior in the implementation notes.

## 6. Acceptance criteria

This step is complete when:

- the baseline builds and tests pass;
- Copilot has produced a symbol-level audit;
- no code has been modified except optional documentation;
- the developer can identify the current undo owner and the current structural edit path;
- all non-negotiable rules are included in the Copilot prompt for later steps.
