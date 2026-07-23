# CPSSim Interactive Architecture Editor — Implementation Guide

## Purpose

This document bundle defines the controlled implementation and completion plan for the CPSSim **Architecture** editor.

The useful interaction work has now been implemented:

- one shared structural edit and Undo/Redo path;
- Architecture actions, context menus, and scoped keyboard commands;
- stable task input/output ports;
- interactive Communication and Logical link creation and deletion;
- cross-view structural selection synchronization;
- persistence and lifecycle support for editable Generic projects;
- protection of Bosch adapter-owned structure.

The remaining work is a final consolidation, regression, acceptance, and documentation pass. It is not another GUI feature-development phase.

Step 6 (Center View and initial graph centering) has been completed separately.

The instructions remain intentionally strict because they are intended for GitHub Copilot, Codex, or another coding agent that may otherwise introduce duplicate state, bypass Undo, or repair only one GUI path.

## Repository baseline

This revision was prepared against:

```text
Repository: gaochuanchao/CPSSim
Branch: main
HEAD: 99a61929a57f7c3ce446d9f355bb75b6b7f162b9
Commit message: update docs
```

Before starting the remaining step, inspect the latest `main` branch and report the exact HEAD SHA. A different SHA is acceptable, but all relevant symbols must be re-read before diagnosis or modification.

At minimum, inspect the current versions of:

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
```

Follow current symbols rather than historical line numbers.

## Current Architecture behavior

The current implementation supports:

- rendering task nodes and semantic links through QtNodes;
- selecting task nodes and links;
- synchronizing selection with Experiment Explorer and System Builder;
- moving task nodes;
- snap-to-grid;
- persistence of task-node positions;
- Fit, 100%, Auto Layout, and Snap to Grid;
- adding a task from the Architecture toolbar;
- adding a task at an empty-canvas context-menu position;
- editing, duplicating, and deleting a selected task;
- editing and deleting a selected link;
- one stable input port and one stable output port for every task;
- interactive port-drag creation of directed links;
- selecting Communication or Logical before link creation;
- editing Source, Destination, and kind in System Builder;
- deleting newly created and persisted links;
- one shared structural Undo/Redo history;
- structural edit rejection while simulation is running;
- Bosch adapter-owned structural protection.

The graph model is no longer structurally read-only. Its mutation-facing methods act as interaction adapters: they map graphical operations to domain identifiers and delegate persistent changes through the shared structural edit path.

QtNodes remains a graphics and interaction layer. It is not the owner of persistent tasks or links.

## Current ownership boundaries

### Structural truth

```text
EditableSystemDraft
run assignments
StructuralSelection
```

### Workspace-layout truth

```text
GuiArchitectureWorkspace
```

Task-node movement is a workspace edit, not a structural model edit.

### Structural mutation and Undo/Redo owner

```text
QtStructuralEditController
└── one shared structural QUndoStack
```

System Builder, Experiment Explorer, and Architecture must all use this owner for persistent structural mutations.

### Refresh and synchronization owner

```text
QtWorkbenchBridge
```

The bridge coordinates draft restoration, application-state notifications, workspace notifications, and structural-selection synchronization.

### Graphics adapter

```text
QtArchitectureGraphModel
QtNodes scene and graphics objects
```

The graph and scene must remain reconstructible from application and workspace state.

## Current link model

- Links are directed.
- Each ordered task pair may contain at most one link.
- Reverse direction is a different ordered pair.
- Link kinds are `Communication` and `Logical`.
- Both kinds are persisted in Generic projects.
- Communication links use solid rendering.
- Logical links use dashed rendering.
- Logical links have latency zero.
- Logical links generate no network events.
- Communication links may have zero or positive latency.
- Both kinds retain the fixed one-tick send offset.
- Link type is selected before creation in Architecture.
- Existing Source, Destination, and kind are edited in System Builder.
- Link conversion preserves endpoints, selection, persistence, and Undo.
- Bosch adapter-owned dependencies remain protected.

## Scope decision

The former component-library and palette drag/drop proposals have been removed from the active plan.

CPSSim already provides direct workflows:

- Architecture toolbar → Add Task;
- empty-canvas context menu → Add Task at position;
- Experiment Explorer → create Resource;
- Architecture port drag → create Communication or Logical link;
- System Builder → edit the selected entity.

A permanent component palette would duplicate existing commands and reduce property-editor space. Palette drag/drop would add MIME and event-handling complexity without enabling a necessary CPSSim research workflow.

Do not reintroduce those features unless a later design decision identifies a concrete missing component workflow.

## Target behavior

After completing the remaining step, the Architecture interaction phase must provide:

1. one structural source of truth;
2. one workspace-layout source of truth;
3. one structural-selection source of truth;
4. one shared structural Undo/Redo history;
5. consistent Task and Link operations across Architecture, Explorer, and System Builder;
6. correct Communication and Logical behavior across creation, editing, deletion, persistence, and runtime;
7. correct project-policy and run-state restrictions;
8. correct save, close, reopen, project replacement, and Save As behavior;
9. focused automated regression coverage;
10. a completed or explicitly qualified manual acceptance matrix;
11. documentation that matches the implemented architecture.

## Mandatory implementation order

Steps 1–5 are complete. Do not modify them merely to make the documentation look newer.

| Order | Document | Status | Result |
|---|---|---|---|
| 1 | `01_BASELINE_AND_NON_NEGOTIABLE_RULES.md` | Complete | Baseline and ownership constraints established |
| 2 | `02_SHARED_STRUCTURAL_EDIT_CONTROLLER.md` | Complete | One structural edit/Undo owner shared by all views |
| 3 | `03_ARCHITECTURE_ACTIONS_AND_CONTEXT_MENUS.md` | Complete | Add, duplicate, delete, edit, fit, and scoped keyboard commands |
| 4 | `04_NODE_PORT_MODEL_REDESIGN.md` | Complete | Every task has one stable input and one stable output port |
| 5 | `05_INTERACTIVE_CONNECTIONS.md` | Complete | Domain-backed Communication and Logical creation/deletion |
| 6 | `06_CENTER_VIEW_AND_INITIAL_CENTERING.md` | Complete | Center View action and automatic project-open centering |
| 7 | `07_FINAL_CONSOLIDATION_AND_ACCEPTANCE.md` | Remaining | Cross-layer audit, regression, manual acceptance, and documentation |
| 8 | `08_COPILOT_PROMPTS.md` | Supporting | Copy/paste prompts for the remaining work |

Step 6 must be completed before beginning a new simulator or experiment feature.

## Central architectural rule

All persistent structural edits must follow:

```text
User gesture
    ↓
QAction / context menu / port gesture / property editor
    ↓
QtStructuralEditController
    ↓
EditableSystemDraft + run assignments + StructuralSelection
    ↓
validation and bridge notifications
    ↓
Architecture presentation/model rebuild
    ↓
QtNodes scene refresh
```

Never use:

```text
User gesture
    ↓
direct insertion/removal of QtNodes records
    ↓
attempt to synchronize the draft afterward
```

The second design creates two sources of truth and eventually corrupts selection, Undo, persistence, or simulation input.

## Build and test loop

Run from the repository root:

```bash
git status --short
git rev-parse HEAD
cmake --preset qt-gui
cmake --build --preset qt-gui -j
ctest -N --test-dir build/qt-gui
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

Use actual registered test names. Do not assume a test executable name.

## Working method for coding agents

For the remaining step:

1. inspect the latest repository and report the exact HEAD;
2. report the baseline build and test result;
3. search globally across all relevant layers;
4. distinguish confirmed defects from hypotheses;
5. propose a minimal file-change list;
6. add only missing, high-value regression coverage;
7. correct only confirmed issues;
8. run focused tests while developing;
9. run the full test suite before completion;
10. inspect the diff for duplicate state, bypassed Undo, stale code, and unrelated formatting;
11. perform or prepare the manual acceptance matrix;
12. update implementation and handoff documentation;
13. do not commit automatically.

## Completion definition

The Architecture interaction phase is complete only when:

- every persistent structural edit goes through the shared controller;
- one structural `QUndoStack` remains;
- Undo/Redo works across edits originating from all supported views;
- Generic projects remain editable;
- Bosch adapter-owned identities remain protected;
- structural edits are rejected while running;
- task-node positions persist independently as workspace state;
- Communication and Logical semantics remain correct;
- loaded and newly created links behave consistently;
- save/reload and project replacement are verified;
- all registered automated tests pass;
- all applicable critical manual checks pass;
- unavailable checks are explicitly marked not executed;
- no unexplained warning or error remains;
- documentation matches the current code.
