# Step 3 — Architecture Actions, Context Menus, and Keyboard Commands

## Objective

Add safe, reusable commands to the Architecture editor:

- Add Task at cursor;
- Edit selected item;
- Duplicate selected task;
- Delete selected task or connection;
- Fit All;
- Actual Size;
- Auto Layout;
- Snap to Grid;
- keyboard shortcuts scoped to the Architecture editor.

Do not implement interactive port dragging in this step.

## 1. Action ownership

Create one `QAction` per command inside `QtArchitectureView`.

Recommended members:

```cpp
QAction* add_task_action_{nullptr};
QAction* edit_action_{nullptr};
QAction* duplicate_action_{nullptr};
QAction* delete_action_{nullptr};
QAction* fit_action_{nullptr};
QAction* actual_size_action_{nullptr};
QAction* auto_layout_action_{nullptr};
QAction* snap_action_{nullptr};
```

Reuse these actions in:

- the Architecture toolbar;
- canvas/node/connection context menus;
- keyboard shortcuts;
- optional main-window Edit menu insertion.

Do not create one-off context-menu actions with duplicated logic.

## 2. New view methods

Add private methods equivalent to:

```cpp
void build_actions();
void build_toolbar(QVBoxLayout& layout);
void update_action_state();

void show_context_menu(
    const QPoint& viewport_position,
    const QPointF& scene_position);

std::optional<QtNodes::NodeId>
node_at(const QPoint& viewport_position) const;

std::optional<QtNodes::ConnectionId>
connection_at(const QPoint& viewport_position) const;

void select_node_for_command(QtNodes::NodeId node_id);
void select_connection_for_command(
    const QtNodes::ConnectionId& connection_id);

void add_task_at_last_context_position();
void duplicate_selected();
void delete_selected_with_confirmation();
void request_edit_selected();
```

Use project naming conventions if different.

Store:

```cpp
QPointF context_scene_position_;
```

only as a temporary location for the current context-menu command. It is not domain state.

## 3. Extend `QtArchitectureGraphicsView`

The current private subclass only overrides `drawBackground`.

Extend it to forward context-menu requests to `QtArchitectureView`.

Because the subclass is currently defined locally in the `.cpp`, avoid adding a local `Q_OBJECT` class solely for one signal. Use a callback:

```cpp
using ContextMenuHandler =
    std::function<void(const QPoint&, const QPointF&)>;
```

Recommended behavior:

```cpp
void contextMenuEvent(QContextMenuEvent* event) override {
    if (context_menu_handler_) {
        context_menu_handler_(
            event->pos(),
            mapToScene(event->pos()));
        event->accept();
        return;
    }
    QtNodes::GraphicsView::contextMenuEvent(event);
}
```

Add a setter for the callback.

Do not suppress QtNodes behavior for unrelated mouse events.

## 4. Robust item hit-testing

QtNodes nodes contain child graphics items. `itemAt(...)` may return a child rather than the top-level `NodeGraphicsObject`.

Use this algorithm:

1. call `view_->itemAt(viewport_position)`;
2. if null, the context is empty canvas;
3. walk `parentItem()` until null;
4. check `item->type()` against:
   - `QtNodes::NodeGraphicsObject::Type`;
   - `QtNodes::ConnectionGraphicsObject::Type`;
5. cast only after the type matches;
6. extract node ID or connection ID.

Do not assume the first item returned is the node object.

Do not identify a node by caption text.

## 5. Context-menu contents

### Empty canvas

Show:

```text
Add Task
----------------
Fit All
100%
Auto Layout
Snap to Grid
```

`Add Task` uses the exact scene position under the context menu.

Disable Add Task when:

- no editable draft exists;
- simulation is running;
- project policy is not generic/editable.

### Task node

Before showing the menu:

- select the node in the scene;
- update shared `StructuralSelection`;
- notify through the existing bridge path.

Show:

```text
Edit
Duplicate
Delete
----------------
Fit All
```

`Edit` should focus or raise System Builder without inventing a second property editor.

A minimal implementation may:

- ensure the task is selected;
- find the System Builder dock through an injected callback or main-window signal;
- emit a signal such as `editSelectionRequested()` that MainWindow handles by showing/raising the dock.

Do not make Architecture call private methods on `QtSystemBuilderWidget`.

### Existing connection

Before showing the menu:

- select the connection;
- update `StructuralSelection`.

Show:

```text
Edit
Delete
----------------
Fit All
```

Connection deletion remains controller/domain-based. If Step 5 is not yet implemented, disable Delete for connections and include a tooltip/status explanation. Do not call the graph model's read-only delete method yet.

## 6. Add Task command

For a canvas context menu:

```cpp
add_task_at(context_scene_position_);
```

The existing method should already:

- create through the shared edit controller after Step 2;
- choose a non-overlapping position;
- snap to grid;
- persist workspace layout;
- select the new task;
- refresh through the established path.

Do not use view-center placement for the canvas-context command.

## 7. Duplicate command

The duplicate action must:

1. require a selected task;
2. call the shared controller's duplicate method;
3. read the new selected task ID after success;
4. place the new task near the original task, not at `(0,0)`;
5. use `next_available_node_position(...)`;
6. snap the new position;
7. persist workspace layout;
8. notify workspace settings once.

Suggested placement:

```text
original node position + one grid step diagonally
```

Then continue offsetting through `next_available_node_position(...)` until free.

Do not copy raw QtNodes `NodeRecord`.

## 8. Delete command

The Delete action must operate on the shared structural selection.

### Confirmation

Use a confirmation dialog in the view layer:

```text
Delete Task "<name>"?
Related assignments and connections may also be removed.
```

or:

```text
Delete selected connection?
```

The controller itself should not show dialogs.

### Execution

After confirmation:

```cpp
edits_.delete_selected();
```

The existing graphics-independent deletion interaction should remain responsible for cascading structural cleanup.

### Selection after deletion

Use the selection returned/restored by the domain interaction.

Do not retain a deleted node ID in a view member.

## 9. Edit command

The first implementation should not create inline node editing.

`Edit` means:

- update shared selection;
- show/raise System Builder;
- focus the most relevant field if an existing public mechanism supports it.

Do not:

- put `QLineEdit` widgets inside QtNodes nodes;
- change task names directly inside `NodeRecord`;
- duplicate the System Builder form on the canvas.

## 10. Keyboard shortcuts

Recommended shortcuts:

```text
Delete        Delete selected structural item
Ctrl+D        Duplicate selected task
F             Fit All
Ctrl+0        100%
```

Use:

```cpp
action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
addAction(action);
```

on `QtArchitectureView`.

This ensures Delete does not trigger while the user edits text in System Builder.

For the single-letter `F` shortcut, do not trigger while a text editor inside the Architecture view has focus. The current view contains no text editor, but preserve scoped shortcut context.

## 11. Action enablement

Create one method:

```cpp
void QtArchitectureView::update_action_state();
```

It should consider:

```text
has editable system
run state
project edit policy
selected item kind
connection-edit availability
```

Call it after:

- selection changes;
- draft changes;
- application state changes;
- project changes;
- context-menu target selection.

Do not scatter policy checks across every QAction lambda.

Suggested logic:

```text
can_structurally_edit =
    editable system exists
    AND run state != Running
    AND edit policy == Generic

Add Task:
    can_structurally_edit

Duplicate:
    can_structurally_edit AND selected task exists

Delete task:
    can_structurally_edit AND selected task exists

Delete connection:
    can_structurally_edit
    AND selected connection exists
    AND connection deletion implemented

Edit:
    any valid selected structural item
```

## 12. Toolbar migration

Replace locally scoped toolbar actions with the new members.

Current actions such as Fit, 100%, Auto Layout, and Snap to Grid should retain:

- object names;
- behavior;
- check state;
- existing tests where possible.

Do not create duplicate toolbar actions.

Add Task may be added to the toolbar after a separator.

Keep the toolbar compact.

## 13. Main-window dock raising

Add a narrow signal from Architecture:

```cpp
void editSelectionRequested();
```

In `QtMainWindow::bind_workbench(...)`, connect it to a lambda that:

1. finds the System Builder dock;
2. shows it if hidden;
3. raises it if tabified;
4. gives focus to the System Builder widget.

Do not pass a raw main-window pointer into Architecture solely for dock manipulation.

## 14. Tests

Extend or add a Qt Architecture interaction test.

Required test cases:

### Context menu hit testing

Prefer testing helper methods or action state rather than fragile pixel-perfect popup interaction.

- empty scene position resolves to no node/connection;
- node location resolves to node ID;
- connection location resolves to connection ID where feasible.

### Add action

- open generic project;
- trigger Add Task at known scene position;
- verify one task added;
- verify selected task is the new task;
- verify workspace position is near requested position and snapped;
- undo removes it.

### Duplicate action

- select task;
- trigger duplicate;
- verify new task;
- verify non-overlapping position;
- undo/redo.

### Delete action

Avoid real modal dialogs in unit tests by extracting a non-dialog execution helper.

- select task;
- invoke confirmed-delete helper;
- verify deletion;
- undo restores.

### Protected/running states

- verify Add/Duplicate/Delete actions disabled.

### Shortcut context

- verify actions use `Qt::WidgetWithChildrenShortcut`.

## 15. Manual acceptance

1. Right-click empty canvas.
2. Add Task.
3. Confirm node appears at clicked location.
4. Undo and redo.
5. Right-click a task.
6. Duplicate.
7. Confirm duplicate appears nearby without overlap.
8. Delete duplicate.
9. Confirm deletion dialog.
10. Press Delete while canvas focused.
11. Edit text in System Builder and press Delete.
12. Confirm text editing is unaffected by graph Delete action.
13. Right-click task and choose Edit.
14. Confirm System Builder becomes visible and shows the task.
15. Repeat in dark and light themes.

## 16. Prohibited changes

Copilot must not:

- implement port dragging yet;
- modify `EditableSystemDraft` directly from the context-menu lambda;
- create a second selection model;
- create a second property editor;
- use global application shortcuts for Delete;
- change node painter visuals in this step;
- remove current grid, snap, fit, or layout behavior;
- bypass the shared controller.
