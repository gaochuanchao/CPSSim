# Step 5 — Interactive Connection Creation and Deletion

## Objective

Allow the user to:

```text
drag from a task output port
to another task input port
```

and create a real CPSSim structural connection through the domain-edit path.

Also allow deletion of an existing selected connection.

## Critical warning

QtNodes must not become the owner of routes.

The graph model receives an interaction request, maps graphical IDs to domain IDs, and calls a controller callback. The controller/domain interaction mutates the draft. The graph then rebuilds from the changed draft.

## 1. Audit the existing route domain API first

Before writing GUI code, Copilot must inspect:

```text
DraftMessageRoute
DraftMessageRouteKey
EditableSystemDraft route methods
SystemExplorerInteraction connection creation
SystemExplorerInteraction deletion behavior
GuiConnectionId
GuiConnectionKind
build_architecture_graph(...)
StructuralSelection::select_connection(...)
StructuralSelection::select_message_route(...)
```

Copilot must answer:

1. Does generic CPSSim support only message routes, or both logical and communication connections?
2. What default fields are used by the existing Create Connection command?
3. How are source and destination selected when creating a connection today?
4. Are self-loops valid?
5. Are duplicate ordered endpoint pairs valid?
6. How is a displayed `GuiConnectionId` mapped back to `DraftMessageRouteKey`?
7. Which project policies allow route creation/deletion?

Do not proceed until these are known.

## 2. Domain-layer interaction extension

If the existing graphics-independent interaction API cannot create a route for explicit endpoints, add a narrow method there.

Preferred location:

```text
src/cpssim/gui/system_builder_interaction.hpp
src/cpssim/gui/system_builder_interaction.cpp
```

Recommended semantic API:

```cpp
SystemBuilderInteractionResult create_message_route(
    TaskId source,
    TaskId destination,
    EditableSystemDraft& draft,
    StructuralSelection& selection);
```

or equivalent.

Requirements:

- validate source exists;
- validate destination exists;
- reject duplicate ordered endpoints;
- apply existing default send offset/delay semantics;
- select the newly created route;
- return diagnostic on failure;
- do not emit Qt signals;
- remain graphics-independent;
- have unit tests in `tests/gui/system_builder_interaction_test.cpp`.

Do not push directly into a public route vector from the Qt model.

### Logical versus communication semantics

Do not silently invent a new connection kind.

Use the currently supported generic-project connection type.

If both types are already supported, the first version should use an explicit small choice UI after the drag rather than guessing.

If only message routes are editable, label the action and tooltip accordingly.

## 3. Extend the shared controller

Add controller operations equivalent to:

```cpp
bool create_connection(
    TaskId source,
    TaskId destination);

bool delete_selected_connection();
```

or:

```cpp
bool delete_connection(GuiConnectionId id);
```

Use `apply(...)` so connection creation/deletion participates in the same undo history.

The mutator should call the graphics-independent interaction method.

On failure:

- do not push an undo command;
- set application status;
- emit status notification through the bridge.

## 4. Add graph-model callbacks

In `QtArchitectureGraphModel`, add callbacks rather than a bridge/controller dependency.

Recommended types:

```cpp
using ConnectionCreateRequested =
    std::function<bool(TaskId, TaskId)>;

using ConnectionDeleteRequested =
    std::function<bool(GuiConnectionId)>;

using StructuralEditEnabled =
    std::function<bool()>;
```

Recommended setters:

```cpp
void set_connection_create_requested(
    ConnectionCreateRequested callback);

void set_connection_delete_requested(
    ConnectionDeleteRequested callback);

void set_structural_edit_enabled(
    StructuralEditEnabled callback);
```

The graph model should remain unaware of:

```text
QtWorkbenchBridge
QtStructuralEditController
QMessageBox
QDockWidget
```

## 5. `connectionPossible(...)`

Implement conservative validation.

Return false when:

- callback is absent;
- structural editing is disabled;
- either node does not exist;
- source and destination entity are not tasks;
- output port index is not `0`;
- input port index is not `0`;
- source equals destination and self-loops are not supported;
- an existing connection already has the same source/destination;
- connection orientation is invalid.

Return true only for a domain request that is plausible.

Do not mutate state in `connectionPossible(...)`.

## 6. `addConnection(...)`

Required behavior:

1. validate again;
2. map `outNodeId` to source `TaskId`;
3. map `inNodeId` to destination `TaskId`;
4. invoke `connection_create_requested_(source, destination)`;
5. do not append to `connections_` as authoritative state;
6. rely on draft-change notification to rebuild;
7. report failure through the controller/application status path.

### Synchronous refresh concern

The QtNodes library may expect model signals during `addConnection(...)`.

Copilot must inspect the exact QtNodes version fetched by this repository and determine whether:

- the controller mutation and bridge signal synchronously rebuild the model; or
- the model must emit a transient signal after successful callback.

Do not guess.

Add a focused integration test that simulates or directly invokes `addConnection(...)` and confirms exactly one visible/domain connection after refresh.

Never both:

```text
append a local connection
AND rebuild the same connection from domain
```

because that creates duplicates.

## 7. `deleteConnection(...)`

Required behavior:

1. find the `GuiConnectionId` using `connection_for(...)`;
2. reject when no domain mapping exists;
3. invoke the delete callback/controller;
4. do not erase only the graphics cache;
5. return the callback result;
6. rely on domain refresh for final scene state.

For non-domain or decorative edges, deletion must be disabled.

## 8. View wiring

In `QtArchitectureView` constructor:

```cpp
model_.set_structural_edit_enabled(
    [this] { return edits_.editing_enabled()
                 && edits_.edit_policy() == Generic; });

model_.set_connection_create_requested(
    [this](TaskId source, TaskId destination) {
        return edits_.create_connection(source, destination);
    });

model_.set_connection_delete_requested(
    [this](GuiConnectionId id) {
        return edits_.delete_connection(id);
    });
```

Use actual project enum names.

Do not capture temporary local references.

## 9. Selection after creation

After a successful connection creation, the domain interaction should select the new route/connection.

The Architecture refresh should then:

- render it;
- synchronize selection where possible;
- show its fields in System Builder.

Do not manually select a stale temporary QtNodes connection ID before rebuild unless required by the library and proven safe.

## 10. Delete action integration

Enable the Step 3 connection Delete action after this step.

When the user deletes a selected connection:

1. show confirmation;
2. call controller delete;
3. rebuild from draft;
4. selection moves to the domain-defined fallback;
5. Undo restores connection and selection.

Do not call both the context action and `model_.deleteConnection(...)` for the same deletion.

Choose one path:

- QtNodes native deletion callback; or
- Architecture QAction calling the controller.

Recommended:

- native QtNodes deletion calls model callback;
- explicit context-menu Delete calls controller using selected domain connection;
- both ultimately reach the same controller method.

## 11. Connection creation UI details

### Cursor feedback

QtNodes should show its native pending connection line.

Do not implement a second custom rubber-band line.

### Invalid target

Invalid targets should refuse the drop without creating a domain route.

Use application status only when the rejection needs explanation, not for every hover.

### Connection type dialog

Only add a dialog when the domain supports multiple editable connection kinds.

Do not show a dialog for every connection if only one type exists.

### Defaults

Use existing domain defaults. Do not hardcode arbitrary latency or send offset in `architecture_model.cpp`.

## 12. Tests

### Graphics-independent interaction tests

- valid endpoint route creation;
- missing source;
- missing destination;
- duplicate route;
- self-loop according to policy;
- selected route after success;
- protected project behavior handled at controller level.

### Controller tests

- create connection;
- undo;
- redo;
- delete;
- undo deletion;
- running-state rejection;
- adapter-policy rejection.

### Graph-model tests

- valid `connectionPossible`;
- invalid port index;
- invalid node;
- duplicate endpoint rejection;
- callback invoked exactly once;
- callback receives correct TaskIds;
- failed callback does not alter model cache;
- delete callback receives correct `GuiConnectionId`.

### Qt integration test

- create two tasks;
- invoke graph-model connection operation;
- verify draft route count increments exactly once;
- process events;
- verify Architecture model connection count increments exactly once;
- undo and verify both domain and model return to previous state.

## 13. Manual acceptance

1. Open generic project.
2. Ensure simulation is stopped/paused as required.
3. Drag output of Task A to input of Task B.
4. Confirm one connection appears.
5. Confirm System Builder shows connection details when selected.
6. Save and reopen project.
7. Confirm connection persists.
8. Undo and redo creation.
9. Delete connection through context menu.
10. Undo deletion.
11. Try duplicate A→B connection.
12. Confirm rejection.
13. Try invalid reverse-direction drag.
14. Try during Running state.
15. Try in Bosch project.
16. Confirm protected behavior.
17. Repeat in dark/light themes.

## 14. Prohibited changes

Copilot must not:

- edit route vectors directly in `architecture_model.cpp`;
- make the graph model own a bridge/controller;
- add a local connection before domain success;
- create two connections after one drag;
- hardcode new timing semantics;
- bypass undo;
- allow duplicate endpoint routes without a domain design;
- change Bosch hidden handoff semantics;
- delete assignment edges as if they were message routes.
