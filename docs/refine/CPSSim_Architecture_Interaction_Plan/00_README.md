# CPSSim Interactive Architecture Editor — Implementation Guide

## Purpose

This document bundle is a step-by-step implementation plan for making the CPSSim **Architecture** view interactive in a controlled way, following the useful interaction ideas of DesCartes Builder while preserving CPSSim's own domain model and simulation semantics.

The instructions are intentionally strict. They are written for use with GitHub Copilot, Codex, or another code agent that may otherwise make broad or unsafe changes.

## Repository baseline

The plan was prepared against:

```text
Repository: gaochuanchao/CPSSim
Baseline commit: c396614a417caa706c3ef9903a40c01561c52b1d
Baseline message: adjust button
```

Before starting, confirm that the repository has not materially changed around these files:

```text
apps/qt_gui/architecture_view.hpp
apps/qt_gui/architecture_view.cpp
apps/qt_gui/architecture_model.hpp
apps/qt_gui/architecture_model.cpp
apps/qt_gui/system_builder_widget.hpp
apps/qt_gui/system_builder_widget.cpp
apps/qt_gui/main_window.hpp
apps/qt_gui/main_window.cpp
apps/qt_gui/workbench_bridge.hpp
apps/qt_gui/workbench_bridge.cpp
src/cpssim/gui/system_builder_interaction.hpp
src/cpssim/gui/system_builder_interaction.cpp
tests/qt_gui/architecture_model_test.cpp
tests/qt_gui/main_window_test.cpp
tests/gui/system_builder_interaction_test.cpp
CMakeLists.txt
```

If symbols have moved, follow the symbol names rather than blindly following old line numbers.

## Current Architecture behavior

The current implementation already supports:

- rendering task nodes and semantic connections through QtNodes;
- selecting a task node and synchronizing selection with Experiment Explorer/System Builder;
- selecting an existing connection;
- moving nodes;
- snap-to-grid;
- persistence of task-node positions;
- Fit, 100%, Auto Layout, and Snap to Grid actions;
- adding a task through `QtArchitectureView::add_task_at(...)`;
- placing a task created elsewhere near the center of the current canvas.

The current graph model intentionally remains structurally read-only:

```cpp
QtArchitectureGraphModel::addNode(...)
QtArchitectureGraphModel::connectionPossible(...)
QtArchitectureGraphModel::addConnection(...)
QtArchitectureGraphModel::deleteConnection(...)
QtArchitectureGraphModel::deleteNode(...)
```

These methods currently reject or ignore graph-structure edits.

## Target behavior

After completing the bundle, the Architecture view should support:

1. right-clicking empty canvas to add a task at that location;
2. right-clicking a task to edit, duplicate, or delete it;
3. keyboard commands that operate only while the Architecture editor has focus;
4. creating and deleting connections by interacting with ports;
5. a reusable component library in System Builder;
6. adding task nodes near the canvas center;
7. dragging a task component from the library to a specific canvas position;
8. a single shared undo/redo history for structural edits made from either System Builder or Architecture.

## Mandatory implementation order

Do not skip ahead.

| Order | Document | Result |
|---|---|---|
| 1 | `01_BASELINE_AND_NON_NEGOTIABLE_RULES.md` | Baseline verified and constraints understood |
| 2 | `02_SHARED_STRUCTURAL_EDIT_CONTROLLER.md` | One structural edit/undo owner shared by all views |
| 3 | `03_ARCHITECTURE_ACTIONS_AND_CONTEXT_MENUS.md` | Add, duplicate, delete, fit, and keyboard interactions |
| 4 | `04_NODE_PORT_MODEL_REDESIGN.md` | Every task has one stable input and one stable output port |
| 5 | `05_INTERACTIVE_CONNECTIONS.md` | Drag-to-connect and connection deletion update the draft |

Each step must compile and pass tests before starting the next step.

## Central architectural rule

The Architecture scene is a **view**, not the owner of the system structure.

All structural edits must follow this path:

```text
User gesture
    ↓
Qt action/context menu/drag gesture
    ↓
shared Qt structural-edit controller
    ↓
EditableSystemDraft + run assignments + StructuralSelection
    ↓
validation and bridge notifications
    ↓
Architecture presentation/model rebuild
    ↓
QtNodes scene refresh
```

Never implement this path:

```text
User gesture
    ↓
direct insertion/removal of QtNodes NodeRecord or ConnectionId
    ↓
attempt to synchronize the draft afterward
```

That second design creates two sources of truth and will eventually corrupt selection, undo, project persistence, or simulation input.

## Build and test loop

Run from the repository root:

```bash
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest --test-dir build/qt-gui --output-on-failure
```

Then manually run:

```bash
./build/qt-gui/cpssim_gui
```

or:

```bash
make run-gui
```

Do not assume a specific test executable name if CMake has changed. Use `ctest -N --test-dir build/qt-gui` to list registered tests.

## Working method for Copilot

For every step:

1. ask Copilot to inspect the listed files and report the relevant existing symbols;
2. ask it to propose a minimal file-change list;
3. reject any proposal that changes simulation kernel semantics;
4. implement one document only;
5. build;
6. run tests;
7. inspect the diff;
8. remove commented-out old code and unrelated formatting;
9. manually test the user interaction;
10. commit only after the step is stable.

## Completion definition

The work is complete only when:

- every structural edit goes through one shared edit controller;
- undo/redo works regardless of whether the edit originated in System Builder or Architecture;
- generic projects are editable;
- Bosch adapter-owned structural identities remain protected;
- edits are disabled while simulation is running;
- node movement still persists independently as workspace layout;
- no scene-only node or edge survives a refresh;
- all automated tests pass;
- the manual acceptance matrix passes in dark and light themes.
