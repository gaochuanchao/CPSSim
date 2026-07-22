# Step 6 — System Builder Component Library and Add-at-Center Workflow

## Objective

Refine System Builder into two logical areas:

```text
System Builder
├── Selected-item editor
└── Component library
```

The component library should provide a simple DesCartes-style workflow:

1. choose a component;
2. create it through the shared structural controller;
3. place a new task near the current Architecture view center;
4. select it;
5. show its editor.

## 1. UI structure

Use a vertical `QSplitter`:

```text
QSplitter (vertical)
├── existing selected-item editor/scroll area
└── component library panel
```

Do not place the library inside every stacked editor page.

The library remains visible while the selected-item editor changes pages.

## 2. Header changes

In:

```text
apps/qt_gui/system_builder_widget.hpp
```

Add forward declarations/members as needed:

```cpp
class QSplitter;
class QListWidget;
class QAction;
```

Recommended members:

```cpp
QSplitter* splitter_{nullptr};
QWidget* component_library_{nullptr};

QAction* add_task_action_{nullptr};
QAction* add_resource_action_{nullptr};
QAction* add_connection_action_{nullptr};
```

Keep action creation separate from page construction.

## 3. Constructor layout migration

Current System Builder roughly contains:

```text
outer QVBoxLayout
└── QScrollArea
    └── editor/pages
```

Change to:

```text
outer QVBoxLayout
└── QSplitter(Qt::Vertical)
    ├── existing QScrollArea
    └── component library widget
```

Preserve the one-pixel outer panel boundary already implemented.

Recommended splitter setup:

```cpp
splitter_->setChildrenCollapsible(false);
splitter_->setStretchFactor(0, 3);
splitter_->setStretchFactor(1, 1);
splitter_->setSizes({420, 160});
```

Do not hardcode a fixed total height.

## 4. Component library presentation

A simple first version may use:

```text
QGroupBox "Components"
QToolButton or QPushButton rows
```

Recommended actions:

```text
Task
Resource
Connection
```

Object names:

```text
action.systemBuilder.addTask
action.systemBuilder.addResource
action.systemBuilder.addConnection

systemBuilder.componentLibrary
systemBuilder.addTask
systemBuilder.addResource
systemBuilder.addConnection
```

Use normal native buttons or tool buttons.

Do not use custom painted cards in the first implementation.

## 5. Reuse actions

The library buttons should call the same actions that may later appear in menus or Architecture.

For example:

```cpp
button->setDefaultAction(add_task_action_);
```

or connect one action to one button.

Do not duplicate mutation lambdas per button.

## 6. Add Task behavior

The Add Task action should:

1. call `edits_.create_task()`;
2. on success emit `taskCreated(task_id)`;
3. rely on the existing MainWindow connection:
   ```text
   taskCreated → QtArchitectureView::place_task_near_view_center
   ```
4. ensure selection refers to the new task;
5. raise/focus the task editor.

Do not calculate canvas center in System Builder.

Architecture owns placement because it owns the view transform.

## 7. Add Resource behavior

The Add Resource action should:

1. call:
   ```cpp
   edits_.create_component(StructuralSection::Resources);
   ```
2. select the new resource through existing interaction semantics;
3. refresh Explorer/System Builder through bridge signals;
4. not create a graph node;
5. not create a resource container on the Architecture canvas.

## 8. Connection library action

After Step 5, choose one controlled behavior.

Recommended first behavior:

```text
Connection button enables a temporary "Connect Tasks" mode
```

In this mode:

- status text instructs the user to drag from task output to task input;
- Architecture tab is activated;
- no connection is created until endpoints are chosen.

A simpler acceptable behavior is:

- activate Architecture tab;
- show a short informational status message.

Do not create a connection with guessed endpoints.

Do not add an empty route without source/destination.

## 9. Action enablement

Use one method:

```cpp
void update_component_action_state();
```

Rules:

### Add Task

Enabled when:

- editable system exists;
- not running;
- generic edit policy.

### Add Resource

Enabled when:

- editable system exists;
- not running;
- policy permits resource editing.

Use the actual policy rules; do not assume all adapter projects forbid resource edits without checking.

### Add Connection

Enabled when:

- editable system exists;
- not running;
- route structure editable;
- Architecture connection editing is available.

Update after application/draft state changes.

## 10. Preserve selected-item editor behavior

Do not regress:

- system page;
- resource page;
- task page;
- connection page;
- execution profile dialog;
- validation diagnostics;
- responsive form wrapping;
- field widths;
- native button styling;
- read-only/protected help.

The library is an addition, not a rewrite of the editor.

## 11. Main-window wiring

Keep the existing signal connection that places tasks near the view center.

If it currently depends on constructing System Builder after Architecture, preserve that order.

Recommended bind order:

```text
construct controller
construct Architecture
construct System Builder
connect taskCreated to Architecture placement
install widgets in docks/tabs
```

Do not locate Architecture using a global singleton.

## 12. Focus behavior

After Add Task:

- Architecture node becomes selected;
- System Builder task page is shown;
- focus may remain on the library button or move to the first editable task field.

Choose consistent behavior and test it.

After Add Resource:

- Explorer selects resource;
- System Builder resource page shows it.

## 13. Tests

### System Builder widget test

- component library exists;
- action object names stable;
- Add Task action creates one task;
- `taskCreated` emitted once with new ID;
- Add Resource creates one resource;
- actions disabled while running;
- actions disabled for protected policy as appropriate.

### Main-window integration

- trigger Add Task action;
- process events;
- verify Architecture model contains task;
- verify position is near current view center;
- verify no overlap with existing nodes;
- undo removes task;
- redo restores and places consistently.

### Regression

- selected-item pages still update;
- component library remains visible across selections;
- narrow System Builder dock remains usable;
- splitter does not collapse editor or library completely.

## 14. Manual acceptance

1. Resize System Builder narrow and wide.
2. Confirm splitter and both regions remain usable.
3. Click Task.
4. Confirm new node near canvas center.
5. Click Task repeatedly.
6. Confirm nodes avoid overlap.
7. Click Resource.
8. Confirm no resource node appears on canvas.
9. Select created resource and edit it.
10. Undo/redo mixed task and resource creation.
11. Run simulation.
12. Confirm structural add actions disabled.
13. Open Bosch project.
14. Confirm policy behavior.
15. Restart app and confirm normal dock layout persistence.

## 15. Prohibited changes

Copilot must not:

- place resource nodes on the flat task graph;
- create a second undo stack;
- create components by inserting Qt list items only;
- create a connection without endpoints;
- remove the existing selected editor;
- add fixed heights that break high-DPI layouts;
- use drag-and-drop in this step;
- modify simulation semantics.
