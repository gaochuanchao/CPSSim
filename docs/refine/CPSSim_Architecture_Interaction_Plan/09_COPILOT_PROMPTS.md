# Step 9 — Copy/Paste Prompts for Copilot

Use one prompt at a time. Do not give Copilot the entire project at once and ask it to "make Architecture interactive."

Before each prompt, ensure the previous step is committed or otherwise safely checkpointed.

---

# Prompt A — Baseline audit only

```text
You are working in repository gaochuanchao/CPSSim.

Do not modify code yet.

Read:
- docs/refine/G7_QT_GUI_MIGRATION_QTNODES_FLAT_ASSIGNMENT.md
- docs/refine/G7_DESCARTES_BUILDER_REFERENCE_NOTES.md
- apps/qt_gui/architecture_view.hpp/.cpp
- apps/qt_gui/architecture_model.hpp/.cpp
- apps/qt_gui/system_builder_widget.hpp/.cpp
- apps/qt_gui/main_window.hpp/.cpp
- apps/qt_gui/workbench_bridge.hpp/.cpp
- src/cpssim/gui/system_builder_interaction.hpp/.cpp
- tests/qt_gui/architecture_model_test.cpp
- tests/qt_gui/main_window_test.cpp
- tests/gui/system_builder_interaction_test.cpp

Report:
1. the source of structural truth;
2. the source of graph-layout truth;
3. the current selection owner;
4. the current undo owner;
5. every current structural edit path;
6. every current Architecture interaction;
7. project-policy and run-state restrictions;
8. the exact files that would change to extract a shared edit controller.

Non-negotiable rules:
- do not change simulation semantics;
- do not make QtNodes the domain source of truth;
- do not create a second undo stack;
- do not bypass Bosch/adapter protection;
- do not make unrelated formatting changes.

Run the existing Qt build and tests and report failures. Stop after the report.
```

---

# Prompt B — Shared structural edit controller

```text
Implement only Step 2 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/02_SHARED_STRUCTURAL_EDIT_CONTROLLER.md

Before editing, summarize the current implementations of:
- DraftState
- RestoreDraftCommand
- QtSystemBuilderWidget::push_mutation
- create_component
- duplicate_selected
- delete_selected
- QtArchitectureView::add_task_at
- QtMainWindow undo/redo wiring

Then implement a QtStructuralEditController in:
- apps/qt_gui/structural_edit_controller.hpp
- apps/qt_gui/structural_edit_controller.cpp

Requirements:
- exactly one structural QUndoStack;
- move existing detached before/after restore-command semantics; do not redesign undo;
- System Builder and Architecture both receive the controller by reference;
- Architecture task creation uses the controller;
- node placement remains Architecture workspace logic;
- MainWindow owns the controller and connects Undo/Redo to it;
- no controller dependency on QWidget dialogs or Architecture;
- clear history only when active project root changes;
- preserve all edit-policy and running-state rules;
- remove old duplicate code; do not leave commented-out code;
- add focused tests and CMake registration.

Do not implement context menus, ports, connection dragging, component library, or drag/drop.

Build and run:
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest --test-dir build/qt-gui --output-on-failure

Show:
1. files changed;
2. key design decisions;
3. test results;
4. remaining risks.
```

---

# Prompt C — Architecture actions and context menus

```text
Implement only Step 3 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/03_ARCHITECTURE_ACTIONS_AND_CONTEXT_MENUS.md

Assume the shared QtStructuralEditController already exists.

Requirements:
- one QAction per command, reused by toolbar/context menu/shortcut;
- right-click empty canvas: Add Task, Fit, 100%, Auto Layout, Snap;
- right-click task: Edit, Duplicate, Delete;
- right-click connection: Edit and Delete, but keep connection Delete disabled until domain deletion is implemented;
- robustly walk QGraphicsItem parent chain to identify QtNodes node/connection objects;
- Add Task uses clicked scene position;
- Duplicate uses controller and places new task near original without overlap;
- Delete uses a view-layer confirmation and controller;
- Edit emits a narrow signal that MainWindow uses to show/raise System Builder;
- Delete/Ctrl+D/F/Ctrl+0 use WidgetWithChildrenShortcut;
- no global Delete shortcut;
- no inline text editors inside graph nodes;
- no direct draft mutation in the view/model;
- no port or connection creation changes in this step;
- preserve existing object names for current toolbar actions;
- add tests for action state, task add/duplicate/delete helpers, and protected/running states.

Do not reformat unrelated code.
Build and run all Qt tests.
```

---

# Prompt D — Stable task ports

```text
Implement only Step 4 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/04_NODE_PORT_MODEL_REDESIGN.md

Requirements:
- every task node always has exactly one input port and one output port;
- both use port index 0;
- both use ConnectionPolicy::Many;
- all existing displayed structural edges use source port 0 and destination port 0;
- remove degree-derived port numbering;
- preserve mapping from QtNodes ConnectionId to GuiConnectionId;
- reject/flag any existing parallel same-endpoint data rather than inventing ports;
- keep connectionPossible false, addConnection no-op, and deleteConnection false for this step;
- verify custom painter does not clip ports;
- extend architecture_model_test for isolated nodes, fan-in, fan-out, invalid port index, and rebuild stability.

Do not implement domain connection creation/deletion.
Build and run all tests.
```

---

# Prompt E — Interactive connections

```text
Implement only Step 5 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/05_INTERACTIVE_CONNECTIONS.md

First perform and report the required domain API audit:
- DraftMessageRoute and key;
- existing connection creation defaults;
- duplicate/self-loop rules;
- mapping between GuiConnectionId and draft route key;
- editable project policies.

Do not edit until the audit is reported.

Then:
- add a narrow graphics-independent explicit-endpoint route creation operation only if missing;
- unit-test it;
- expose create/delete connection through QtStructuralEditController and the existing undo stack;
- add callbacks to QtArchitectureGraphModel, not a bridge/controller dependency;
- connectionPossible validates nodes, port 0, policy, duplicates, and self-loop rule;
- addConnection maps node IDs to TaskIds and calls the callback;
- deleteConnection maps to GuiConnectionId and calls the callback;
- do not mutate the graphics cache as domain truth;
- rely on domain/bridge refresh and verify no duplicate visible edge;
- inspect the exact fetched QtNodes API before deciding which model signals are required;
- enable connection Delete context action;
- add domain, controller, model, and integration tests;
- preserve Bosch hidden handoff and all simulation semantics.

Stop if the domain cannot represent the requested connection without a design decision. Do not invent timing defaults.
Build and run all tests.
```

---

# Prompt F — Component library

```text
Implement only Step 6 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/06_COMPONENT_LIBRARY_AND_ADD_AT_CENTER.md

Requirements:
- use a vertical QSplitter;
- top: existing selected-item editor;
- bottom: compact component library;
- actions for Task, Resource, Connection;
- reuse QActions; do not duplicate mutation lambdas;
- Task calls shared controller and emits taskCreated once;
- MainWindow/Architecture places new task near current view center;
- Resource creates domain resource only, never a graph/resource container;
- Connection action only activates/instructs connect mode; it must not guess endpoints;
- actions follow editable-system, running-state, and project-policy rules;
- preserve responsive forms, execution-profile dialog, boundaries, and diagnostics;
- add object names and tests;
- no drag/drop yet.

Build and run all tests.
```

---

# Prompt G — Drag/drop Task to canvas

```text
Implement only Step 7 from:
docs/refine/CPSSim_Architecture_Interaction_Plan/07_DRAG_AND_DROP_TO_CANVAS.md

Requirements:
- MIME type application/x-cpssim-component;
- stable payload "task";
- CopyAction;
- Architecture accepts only task payload;
- create only in dropEvent, never dragEnter/dragMove;
- convert viewport drop position with mapToScene;
- call existing add_task_at(scene_position);
- use shared controller and undo;
- use existing snap and collision-avoidance functions;
- Resource and Connection drops are rejected;
- private graphics-view subclass forwards drop requests using callbacks, not bridge/controller references;
- no temporary scene/domain node preview;
- test MIME, policy rejection, coordinate transform, one-task creation, and undo.

Build and run all tests.
```

---

# Prompt H — Final verification and cleanup

```text
Perform the complete verification in:
docs/refine/CPSSim_Architecture_Interaction_Plan/08_TEST_AND_ACCEPTANCE_MATRIX.md

Do not add new features.

Tasks:
1. run full build and ctest;
2. identify missing automated coverage and add only the minimum necessary tests;
3. inspect for duplicate undo stacks;
4. inspect for direct domain mutation in Architecture graph/view;
5. inspect for scene-only structural edits;
6. inspect shortcut contexts;
7. inspect project-policy and running-state checks;
8. remove commented-out old code and unrelated TODOs;
9. confirm CMake source/test registration;
10. update relevant GUI architecture and implementation documentation.

Report every manual test that still requires human execution.
Do not commit automatically.
```

---

# Emergency correction prompt

Use this when Copilot begins over-refactoring:

```text
Stop. Revert all unrelated changes.

The current task is limited to the named step document. Preserve:
- simulation kernel semantics;
- EditableSystemDraft as structural truth;
- GuiArchitectureWorkspace as position truth;
- StructuralSelection as selection truth;
- one shared structural QUndoStack;
- bridge-driven refresh;
- adapter-owned project protection;
- existing QtNodes painter and presentation unless the step explicitly changes them.

Before continuing, show a minimal file-change list and explain why each file is necessary.
```
