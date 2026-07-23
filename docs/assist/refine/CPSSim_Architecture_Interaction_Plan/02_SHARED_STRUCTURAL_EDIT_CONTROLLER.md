# Step 2 — Extract a Shared Structural Edit Controller

## Objective

Create one Qt-layer controller that owns:

- the structural `QUndoStack`;
- the `DraftState` snapshot used for undo/redo;
- the `RestoreDraftCommand`;
- generic structural mutation execution;
- create, duplicate, and delete commands;
- edit-policy and run-state checks shared by System Builder and Architecture.

This step fixes the most important current architectural weakness: System Builder owns the undo stack, while `QtArchitectureView::add_task_at(...)` performs a separate direct edit path.

## 1. New files

Create:

```text
apps/qt_gui/structural_edit_controller.hpp
apps/qt_gui/structural_edit_controller.cpp
```

Add them to the same Qt GUI library/executable source list that currently includes:

```text
architecture_view.cpp
architecture_model.cpp
system_builder_widget.cpp
main_window.cpp
```

Do not create a new library unless the existing CMake organization clearly requires one.

## 2. Class responsibility

Use a name equivalent to:

```cpp
QtStructuralEditController
```

Recommended declaration shape:

```cpp
class QtStructuralEditController final : public QObject {
    Q_OBJECT

  public:
    using DraftMutator =
        std::function<void(
            EditableSystemDraft&,
            std::vector<DraftTaskAssignment>&,
            StructuralSelection&)>;

    explicit QtStructuralEditController(
        QtWorkbenchBridge& bridge,
        QObject* parent = nullptr);

    QUndoStack& undo_stack() noexcept;

    bool editing_enabled() const;
    ProjectSystemEditPolicy edit_policy() const;

    bool apply(
        const QString& command_text,
        DraftMutator mutator);

    bool create_component(StructuralSection section);
    std::optional<TaskId> create_task();
    bool duplicate_selected();
    bool delete_selected();

    void synchronize_active_project();

  private:
    QtWorkbenchBridge& bridge_;
    QUndoStack undo_stack_;
    std::optional<std::filesystem::path> undo_project_root_;
};
```

Names may be adjusted to match project conventions, but the responsibility must remain narrow and clear.

## 3. Move existing implementation, do not rewrite it

Move these concepts out of `system_builder_widget.cpp`:

```text
DraftState
RestoreDraftCommand
QtSystemBuilderWidget::push_mutation
undo_project_root_
the QUndoStack allocation/ownership
the core create/duplicate/delete mutation logic
```

The controller should preserve the existing command semantics.

Do not replace the current safe detached-state approach with incremental inverse operations.

The undo command should continue restoring the complete detached state:

```text
EditableSystemDraft
run assignments
StructuralSelection
```

through the bridge's existing restore path.

## 4. Restore command requirements

The moved `RestoreDraftCommand` must:

- store `before` and `after`;
- call the existing bridge restore method;
- avoid directly emitting unrelated GUI signals;
- rely on the established bridge/application synchronization path;
- have stable command text for the Undo menu;
- not retain references to temporary vectors or local lambdas.

Recommended outline:

```cpp
class RestoreDraftCommand final : public QUndoCommand {
  public:
    RestoreDraftCommand(
        QtWorkbenchBridge& bridge,
        DraftState before,
        DraftState after,
        QString text);

    void undo() override;
    void redo() override;

  private:
    void restore(const DraftState& state);

    QtWorkbenchBridge& bridge_;
    DraftState before_;
    DraftState after_;
};
```

## 5. `apply(...)` behavior

The controller's generic mutation function must perform these operations in order:

1. reject when no editable system exists;
2. reject structural edits while running;
3. synchronize/clear undo history if the active project root changed;
4. snapshot current draft, assignments, and selection into `before`;
5. copy `before` to `after`;
6. run the provided mutator on `after`;
7. catch domain exceptions;
8. push one restore command containing `before` and `after`;
9. return success/failure;
10. report failures through the existing application status/bridge status signal path.

Do not mutate the active draft before the undo command is pushed.

Do not capture references into `after` that outlive the function.

## 6. Project-change behavior

The existing System Builder code clears the undo stack when the active project root changes. Preserve this behavior in the controller.

The controller must also behave correctly when:

- no project is active;
- a project is closed;
- a different project is opened;
- Save As changes the active root;
- the home screen is shown;
- the same project is refreshed without changing roots.

Do not clear undo history on every ordinary refresh.

## 7. Migrate System Builder

### Header changes

In:

```text
apps/qt_gui/system_builder_widget.hpp
```

Change the constructor from taking only the bridge to taking both bridge and controller:

```cpp
QtSystemBuilderWidget(
    QtWorkbenchBridge& bridge,
    QtStructuralEditController& edits,
    QWidget* parent = nullptr);
```

Add a forward declaration for the controller.

Replace:

```cpp
QUndoStack* undo_stack_;
std::optional<std::filesystem::path> undo_project_root_;
```

with:

```cpp
QtStructuralEditController& edits_;
```

Remove the System Builder `undo_stack()` accessor after main-window wiring has been migrated. A temporary forwarding accessor is acceptable only within the same step and must be removed before completion.

### CPP changes

All existing field-edit paths must call:

```cpp
edits_.apply(...)
```

instead of:

```cpp
push_mutation(...)
```

All existing create/duplicate/delete UI operations must call controller methods.

Do not change validation rules, field parsing, edit policy, or diagnostic messages in this step.

### Task-created signal

The existing `taskCreated(TaskId)` signal may remain.

When System Builder successfully creates a task:

1. use `edits_.create_task()` or `edits_.create_component(Tasks)`;
2. read the newly selected task ID;
3. emit `taskCreated(task_id)` exactly once.

Do not emit on failure.

## 8. Migrate Architecture task creation

Change:

```text
QtArchitectureView::add_task_at(...)
```

so it no longer calls the application's explorer interaction directly.

It must call the shared controller:

```cpp
const auto task_id = edits_.create_task();
```

Then it may:

- calculate the free scene position;
- snap the position;
- persist the workspace layout;
- refresh/select the new task.

The structural creation belongs to the controller. The graphical placement belongs to Architecture.

This separation is important:

```text
Controller:
    create task identity and default structural state

Architecture:
    choose visual placement in the workspace
```

## 9. Main-window ownership

In:

```text
apps/qt_gui/main_window.hpp
apps/qt_gui/main_window.cpp
```

Add one controller member owned by the main window.

Preferred lifetime:

```cpp
QtStructuralEditController* structural_edits_{nullptr};
```

created with `this` as QObject parent after the workbench bridge is available.

In `QtMainWindow::bind_workbench(...)`:

1. set `bridge_`;
2. construct `structural_edits_`;
3. construct Architecture with bridge + controller;
4. construct System Builder with bridge + controller;
5. connect undo/redo actions to `structural_edits_->undo_stack()`;
6. connect `canUndoChanged` and `canRedoChanged` from that stack;
7. remove all dependency on `system_builder_->undo_stack()`.

Do not let Architecture own the controller.

Do not create the controller before `bridge_` exists.

## 10. Constructor changes

Recommended Architecture constructor:

```cpp
QtArchitectureView(
    QtWorkbenchBridge& bridge,
    QtStructuralEditController& edits,
    QWidget* parent = nullptr);
```

Recommended System Builder constructor:

```cpp
QtSystemBuilderWidget(
    QtWorkbenchBridge& bridge,
    QtStructuralEditController& edits,
    QWidget* parent = nullptr);
```

Store references:

```cpp
QtWorkbenchBridge& bridge_;
QtStructuralEditController& edits_;
```

## 11. Signals and refresh behavior

A successful structural command must still trigger the established draft-change path.

Do not add direct calls from controller to:

```text
QtArchitectureView::refresh
QtSystemBuilderWidget::refresh
QtExperimentExplorerWidget::refresh
```

Those widgets already subscribe to bridge/application signals.

If a moved command no longer refreshes all views, fix the bridge notification path rather than adding view-to-view calls.

## 12. Tests

Create:

```text
tests/qt_gui/structural_edit_controller_test.cpp
```

or extend an existing suitable Qt test target.

Required test cases:

### Apply and undo

- create a generic editable project;
- call controller `apply(...)` with a small safe mutation;
- verify draft changed;
- call undo;
- verify exact prior draft state;
- call redo;
- verify changed state.

### Task creation

- record task count;
- call `create_task()`;
- verify task count increased by one;
- verify selection refers to the new task;
- undo;
- verify count and selection restored;
- redo;
- verify new task returns.

### Duplicate

- select a task;
- duplicate;
- verify exactly one additional task;
- verify new selection;
- undo/redo.

### Delete

- select a deletable task;
- delete;
- verify related assignments/routes are handled by existing interaction logic;
- undo restores all affected state.

### Protected project

- use a project with read-only adapter policy;
- verify task creation/duplicate/delete fail;
- verify undo stack count does not change.

### Running state

- set or simulate running state through existing test mechanisms;
- verify structural methods fail;
- verify undo stack count does not change.

### Project switch

- create command in project A;
- switch to project B;
- synchronize controller;
- verify old undo history is cleared.

## 13. Regression tests

Update existing tests that currently call:

```cpp
system_builder_->undo_stack()
```

to use the shared controller or main-window undo actions.

Verify:

- main-window Undo/Redo enablement still works;
- field edits remain undoable;
- Architecture add-task becomes undoable;
- taskCreated still places a node near center;
- project close/open does not retain stale commands.

## 14. Prohibited shortcuts

Copilot must not:

- leave `RestoreDraftCommand` duplicated in two files;
- leave a second `QUndoStack` in System Builder;
- call System Builder methods from Architecture;
- make the controller depend on QWidget dialogs;
- move the controller into the simulation core;
- change `EditableSystemDraft` storage solely for the GUI;
- include `architecture_view.hpp` from the controller;
- directly update QtNodes from the controller.

## 15. Acceptance criteria

This step is complete when:

- one structural undo stack exists;
- System Builder field edits use the controller;
- System Builder create/duplicate/delete use the controller;
- Architecture `add_task_at` uses the controller;
- Undo/Redo actions connect to the controller;
- all views still refresh through bridge signals;
- automated tests pass;
- manual task creation from System Builder and Architecture can both be undone in one chronological history.
