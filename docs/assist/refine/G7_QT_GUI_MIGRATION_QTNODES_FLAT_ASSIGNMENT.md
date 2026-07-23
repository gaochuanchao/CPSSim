# Goal 7 — Qt GUI Migration, QtNodes Architecture Editor, and Flat Resource-Assignment Presentation

## 1. Goal

Goal 7 introduces a native C++ Qt 6 Widgets frontend for CPSSim and uses QtNodes as the foundation of the Architecture editor.

The implementation must reuse the graphics-independent structures completed in Goal 6. It must not rewrite the simulation kernel or change simulation semantics.

The selected Architecture design is:

```text
flat task graph
+ independent task nodes
+ logical and communication connections
+ resource-colored task decoration
+ explicit resource-name badge
+ synchronized task-to-resource mapping panel
```

Resource assignment is not shown through large resource containers. Tasks assigned to the same resource share a stable resource accent color, but every task also displays the resource name so that assignment is never communicated by color alone.

The long-term structure is:

```text
cpssim_core
    deterministic simulation semantics
        ↓
graphics-independent application and presentation layer
        ↓
cpssim_qt_gui
    Qt Widgets workbench
    QtNodes Architecture editor
    native integrated plotting
        ↓ optional separate process
external scientific plotter
    Python + PySide6 + Matplotlib
```

The existing Dear ImGui frontend remains available until the Qt frontend passes functional and performance parity.

---

## 2. Baseline

Implementation baseline:

```text
repository: gaochuanchao/CPSSim
commit:     05345ec5d5f599a234e22250f26734be7da6b113
```

Goal 6 already provides:

- generation-based presentation publication;
- immutable `SimulationSnapshot`;
- shared immutable `CompletedRunData`;
- managed completed-run finalization;
- graphics-independent Architecture graph and workspace layout;
- structural and runtime selection models;
- cached Canonical Events rows and filtering;
- graphics-independent plot signal projections and downsampling;
- result export services;
- project and workspace persistence;
- performance instrumentation.

Goal 7 must reuse these components.

---

# 3. Selected visual design

## 3.1 Architecture canvas

The Architecture canvas displays tasks as normal independent QtNodes nodes:

```text
┌─────────────────────────────┐
▌ Sensor                CPU0  │
│ T=10 ms  D=10 ms  C=2 ms   │
└─────────────────────────────┘
```

Where:

- the narrow left stripe uses the assigned resource's stable accent color;
- the resource badge displays the resource name, such as `CPU0`;
- the main node body uses the application theme;
- the node remains a first-class graph node with its own ports and connections;
- the node border is reserved for selection, validation, and runtime status.

Example:

```text
[Sensor | CPU0] ───▶ [Estimator | CPU0] ───▶ [Controller | CPU1]
       blue                   blue                    orange
```

## 3.2 Resource mapping panel

Add a dockable `Resource Assignments` panel.

Primary presentation:

```text
Task          Resource      Profile/WCET      Priority      Status
Sensor        CPU0          2 ms              3             Valid
Estimator     CPU0          4 ms              2             Valid
Controller    CPU1          3 ms              1             Valid
Logger        Unassigned    —                 —             Invalid
```

The panel is authoritative for viewing and editing assignment.

Recommended default dock area:

```text
bottom
```

The user may move it to a side area using normal Qt docking.

## 3.3 Resource legend

At the top of the mapping panel, or in a compact collapsible area, show:

```text
■ CPU0    2 tasks
■ CPU1    2 tasks
□ Unassigned    1 task
```

Clicking a resource legend item selects or highlights all tasks assigned to that resource.

## 3.4 No resource containers

Do not implement:

- resource rectangles containing task nodes;
- parent-child task containment;
- dragging a resource to move its assigned tasks;
- resource resize handles on the Architecture canvas;
- assignment lines from tasks to resources.

This eliminates unnecessary scene complexity and keeps task-level dependencies readable.

---

# 4. Visual encoding rules

## 4.1 Resource color

Create a deterministic resource-color service:

```cpp
struct ResourceVisualIdentity {
    ResourceId resource_id;
    std::string display_name;
    QColor accent;
};
```

Mapping requirements:

- stable across reload;
- stable when unrelated resources are added;
- independent of vector order;
- deterministic from `ResourceId`;
- suitable for light and dark themes;
- sufficient contrast against node background;
- no semantic meaning assigned to a particular hue.

Do not persist a random color unless the user explicitly customizes it in a later goal.

Recommended mapping:

```text
stable hash(ResourceId)
    → palette index
    → theme-adjusted accent
```

Unassigned tasks use a neutral dashed or gray accent and the visible text `Unassigned`.

## 4.2 Do not rely on color alone

Every task node must display:

- assigned resource name;
- assignment status tooltip;
- stable task name.

The mapping table repeats the resource name and color swatch.

## 4.3 Keep assignment and runtime state separate

Use separate visual channels:

```text
left stripe / resource badge:
    assignment

node border:
    normal, selected, invalid, warning

small status indicator:
    ready, running, blocked, completed, deadline miss

connection style:
    logical or communication
```

A resource color must not be reused as a runtime state color.

## 4.4 Connection styles

Logical connection:

- zero displayed communication latency;
- no MessageSend or MessageDelivery semantics;
- solid or neutral connection style;
- tooltip identifies it as logical.

Communication channel:

- displays configured network latency;
- distinct stroke pattern and/or small communication icon;
- tooltip shows route and latency;
- creates MessageSend and MessageDelivery according to the network model.

The hidden Bosch one-tick adapter handoff remains hidden and is not added to displayed communication latency.

---

# 5. Ideas adopted from DesCartes Builder

DesCartes Builder is used as a reference implementation, not copied as CPSSim's architecture.

Adopt the following interaction and shell ideas.

## 5.1 Central graph tabs

Use a central tab widget capable of hosting Architecture and future graph documents.

Initial CPSSim tabs:

```text
Architecture
Timeline
Signals
Integrated Plot
```

Do not implement multiple independent CPSSim projects in one process during the first migration. The tab structure is for workbench views, not multi-project ownership.

## 5.2 Main-window shell

Use:

```text
QMainWindow
├── menu bar
├── simulation toolbar
├── left vertical view toolbar
├── central tab widget
├── QDockWidget panels
└── bottom status/log area
```

The left vertical toolbar may toggle major docks:

- Explorer;
- System Builder;
- Resource Assignments;
- Runtime Inspector;
- Results.

This is inspired by DesCartes Builder's left toolbar that toggles docked tools.

## 5.3 Node library and editor split

Use a dock with two logical sections:

```text
System Builder
├── selected-item editor
└── component library
```

The selected-item editor appears above the library, separated by a `QSplitter`.

The component library initially contains:

```text
Tasks
Resources
Connections
Physical/FMU components
```

For CPSSim, adding a component must mutate `EditableSystemDraft`, not directly mutate QtNodes.

## 5.4 Add-at-center behavior

Support the simple DesCartes-style workflow:

1. user selects an item in the component library;
2. new item is added near the center of the current Architecture view;
3. if occupied, offset it by a fixed logical grid step until a free location is found;
4. select the newly created item;
5. focus its editor.

Also support drag-and-drop from the library to the canvas as a later enhancement.

## 5.5 Selection-driven property editor

Canvas selection updates the System Builder editor.

Editor changes update the draft model, validate it, and refresh the canvas.

Do not directly edit scene-owned node state without updating the CPSSim draft.

## 5.6 Modified-state indication

When project or workspace edits occur:

- mark the project dirty;
- update the window title;
- enable Save;
- warn on close or project replacement if unapplied/unsaved changes exist.

## 5.7 Menu, toolbar, and shortcuts

Create one `QAction` for each command and reuse it in menus, toolbar buttons, and shortcuts.

Recommended shortcuts:

```text
Ctrl+N       new project
Ctrl+O       open
Ctrl+S       save
Ctrl+Shift+S save as
Ctrl+R       run
Space        pause/resume, only when not editing text
F10          step next event
Ctrl+Z       undo
Ctrl+Shift+Z redo
Delete       delete selected structural item
F            fit Architecture view
```

## 5.8 Bottom diagnostic panel

Add a dockable `Diagnostics` panel for:

- project validation;
- simulation diagnostics;
- export status;
- background finalization failures;
- Qt/plugin/backend errors.

Do not use raw `qDebug()` output as the only user-facing diagnostic channel.

## 5.9 Style configuration

Create centralized style definitions for:

- graph background grid;
- node body;
- selected node;
- invalid node;
- logical connection;
- communication connection;
- resource accent palette.

Do not scatter style-sheet literals across widget constructors.

---

# 6. Ideas deliberately not adopted from DesCartes Builder

## 6.1 No DAG assumption

DesCartes Builder uses a directed acyclic graph model for its pipeline.

CPSSim may contain control feedback and must not assume the editable architecture is acyclic.

Use:

```text
QtNodes::AbstractGraphModel
QtNodes::BasicGraphicsScene
QtNodes::GraphicsView
```

Do not derive the CPSSim graph model from `DirectedAcyclicGraphModel`.

## 6.2 QtNodes is not the domain model

Do not make QtNodes serialization authoritative.

Authoritative state remains:

- `EditableSystemDraft`;
- project model;
- run plan;
- workspace state;
- stable CPSSim IDs.

QtNodes is a view/editor adapter.

## 6.3 No global selected node variable

Selection belongs in:

```text
StructuralSelection
```

The Qt bridge and widgets synchronize through that model.

## 6.4 No per-cell item tables for large data

Do not use `QTableWidget` or `QStandardItemModel` for Canonical Events.

Use `QAbstractTableModel` and immutable cached data.

## 6.5 No direct UI-to-graph mutation

Editor widgets call CPSSim application commands.

The flow is:

```text
widget edit
    → application command
    → EditableSystemDraft mutation
    → validation
    → Architecture graph projection
    → QtNodes adapter refresh
```

## 6.6 No graph-file-in-archive replacement

CPSSim project and workspace formats remain versioned and authoritative.

QtNodes JSON may be inspected for debugging, but project load/save must not depend on QtNodes' internal serialization format.

---

# 7. Target architecture

## 7.1 Build targets

Introduce:

```text
cpssim_core
cpssim_gui_support
cpssim_qt_gui_support
cpssim_qt_gui
cpssim_gui             existing Dear ImGui frontend
```

Temporary frontend options:

```cmake
option(CPSSIM_BUILD_IMGUI_GUI "Build the Dear ImGui frontend" OFF)
option(CPSSIM_BUILD_QT_GUI "Build the Qt Widgets frontend" OFF)
```

Keep compatibility with the existing `CPSSIM_BUILD_GUI` option during migration.

## 7.2 Dependency direction

```text
cpssim_core
    ↑
cpssim_gui_support
    ↑                         ↑
cpssim_gui              cpssim_qt_gui_support
                                  ↑
                           cpssim_qt_gui
```

`cpssim_core` must remain free of Qt.

## 7.3 Qt version baseline

Target Ubuntu 24.04 system Qt:

```text
Qt 6.4.x compatibility baseline
```

Required components:

```cmake
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test OpenGLWidgets)
```

Use `OpenGLWidgets` only where needed.

Do not download Qt with `FetchContent`.

## 7.4 QtNodes dependency

Evaluate:

1. upstream QtNodes;
2. `CPS-research-group/nodeeditor`, the fork used by DesCartes Builder.

The evaluation must compare:

- Qt 6.4 compatibility;
- generic `AbstractGraphModel` stability;
- undo support;
- grid and view behavior;
- custom node painting;
- current maintenance;
- changes unique to the fork;
- license and attribution;
- fixes missing from either side.

Pin the selected dependency to an immutable commit.

Disable dependency examples/tests/docs in normal CPSSim builds.

---

# 8. Workbench application extraction

## 8.1 Current problem

The current Dear ImGui `GuiApplication` mixes:

- application state and lifecycle;
- project workflows;
- session control;
- snapshot publication;
- completed-result finalization;
- export;
- selection;
- Dear ImGui drawing and layout.

Qt must not wrap that class directly.

## 8.2 New controller

Extract a graphics-independent class such as:

```cpp
class WorkbenchApplication {
public:
    // Project/session
    void new_generic_project(...);
    void new_bosch_project(...);
    void open_project(...);
    void save_project();
    void save_project_as(...);
    void close_project();

    // Simulation
    void enqueue(GuiCommand command);
    bool update();
    bool needs_update() const noexcept;

    // State
    GuiRunState run_state() const noexcept;
    const SimulationProgress& progress() const noexcept;
    std::shared_ptr<const SimulationSnapshot> snapshot() const noexcept;
    std::shared_ptr<const RunResult> completed_result() const noexcept;

    // Structural editing
    EditableSystemDraft* editable_system() noexcept;
    void execute(SystemEditCommand command);
    SystemDraftBuildResult validate_system() const;

    // Selection and workspace
    StructuralSelection& structural_selection() noexcept;
    GuiSelection& runtime_selection() noexcept;
    GuiWorkspaceState& workspace() noexcept;

    // Result lifecycle
    bool process_background_publications();
    void cancel_background_work();

    // Diagnostics
    const ApplicationDiagnosticModel& diagnostics() const noexcept;

    // Generations
    std::uint64_t application_generation() const noexcept;
    std::uint64_t presentation_generation() const noexcept;
    std::uint64_t completed_run_generation() const noexcept;
};
```

The exact API may vary, but it must be free of ImGui and Qt.

## 8.3 Qt adapter

Create:

```cpp
class QtWorkbenchBridge final : public QObject {
    Q_OBJECT
};
```

The bridge owns or references `WorkbenchApplication` and exposes Qt signals/slots.

Qt types may be used in the bridge API, but not in the graphics-independent controller.

---

# 9. Qt event-loop integration

## 9.1 Idle behavior

Qt's event loop handles idle waiting.

When Paused, Finished, or NotConfigured:

- no periodic simulation timer runs;
- no repeated UI refresh timer runs;
- Qt waits for native events;
- worker completion is delivered through a queued GUI-thread invocation.

Do not reimplement the Dear ImGui pointer-region or GLFW wait system in Qt.

## 9.2 Live mode

Use a bounded repeating or rescheduled timer around 16 ms.

Each callback:

1. consumes queued commands;
2. performs bounded session update;
3. applies existing generation/publication policy;
4. emits progress and state changes;
5. returns to Qt.

The existing presentation publication cap remains 15 Hz.

## 9.3 Fast mode

Use:

```cpp
QTimer::singleShot(0, bridge, &QtWorkbenchBridge::continue_fast_run);
```

Each continuation performs one existing bounded Fast batch and returns to Qt.

Do not run an unbounded simulation loop inside one slot.

## 9.4 Completed result publication

The current finalizer may remain a `std::jthread`.

Worker completion requests a queued GUI-thread callback using `QMetaObject::invokeMethod`.

The worker never accesses QWidget, Qt models, or selection.

---

# 10. Main-window layout

## 10.1 Central workbench

```text
QStackedWidget
├── HomePage
└── WorkbenchPage
```

Workbench central tabs:

```text
Architecture
Timeline
Signals
Integrated Plot
```

## 10.2 Docks

Default arrangement:

```text
Left:
    Experiment Explorer
    System Builder

Right:
    Run Configuration
    Runtime Inspector

Bottom:
    Resource Assignments
    Resources
    Canonical Events
    Results
    Diagnostics
```

Allow tabbing docks in the same area.

## 10.3 Left view toolbar

Add a narrow non-movable toolbar with checkable actions for major docks.

Each action controls one dock's visibility.

This follows the successful DesCartes Builder pattern while allowing CPSSim users to keep the graph canvas large.

## 10.4 Persistence

Every dock and toolbar must have a stable `objectName`.

Persist:

```cpp
QMainWindow::saveGeometry()
QMainWindow::saveState(version)
```

Store project-specific workspace layout in project workspace state.

Use a version number and fall back safely when restore fails.

---

# 11. QtNodes Architecture model

## 11.1 Adapter class

Create:

```cpp
class QtArchitectureGraphModel final : public QtNodes::AbstractGraphModel
```

It adapts:

- `GuiArchitectureGraph`;
- `EditableSystemDraft`;
- `StructuralSelection`;
- `GuiArchitectureWorkspace`;
- CPSSim validation and edit commands.

## 11.2 Stable ID mapping

CPSSim IDs are authoritative.

Maintain explicit mappings:

```cpp
std::unordered_map<GuiGraphNodeId, QtNodes::NodeId> to_qtnodes_;
std::unordered_map<QtNodes::NodeId, GuiGraphNodeId> from_qtnodes_;
```

Do not truncate 64-bit IDs.

Rebuild mapping deterministically when the graph generation changes.

## 11.3 Task node

Task node content:

```text
title
resource badge
period/deadline/WCET compact row
ports
validation/runtime status
```

Keep normal node height compact.

Node dimensions must not change on every runtime update.

## 11.4 Resource nodes

Resources do not appear as container nodes.

Optional normal resource nodes may be introduced later only for:

- hardware topology;
- network topology;
- explicit resource-to-resource links.

They are not used to represent assignment.

## 11.5 Component creation

Initial component library actions:

- Add Task;
- Add Resource;
- Add Logical Connection;
- Add Communication Channel.

Node creation:

```text
library action
    → SystemEditCommand
    → draft creates entity and stable ID
    → graph projection
    → QtNodes creates visual item at requested logical position
```

## 11.6 Deletion

Deleting a selected entity must:

- check cascading references;
- request confirmation when necessary;
- call existing System Builder workflow;
- repair selection;
- rebuild graph;
- push an undo command where supported.

---

# 12. Resource assignment panel

## 12.1 Table model

Create:

```cpp
class ResourceAssignmentTableModel final : public QAbstractTableModel
```

Suggested columns:

1. color;
2. task;
3. assigned resource;
4. execution profile/WCET;
5. priority;
6. accessibility;
7. validation status.

The model operates on stable task IDs.

## 12.2 Editing

Use a `QStyledItemDelegate` for the Resource column.

The delegate provides a `QComboBox` containing:

- valid accessible resources;
- `Unassigned`.

When selection changes:

```text
table edit
    → assignment command
    → draft mutation
    → validation
    → mapping model refresh
    → task badge/accent refresh
```

Do not mutate table-owned strings as application state.

## 12.3 Grouping and sorting

Provide sort by:

- task name;
- resource;
- status;
- priority.

Optional grouped presentation can be added later with a tree model.

The first implementation should use one table, not duplicate tree and table models.

## 12.4 Bidirectional selection

Canvas task selected:

- select assignment row;
- show task in System Builder.

Assignment row selected:

- select task node;
- center/reveal node;
- show task editor.

Resource legend selected:

- highlight all tasks assigned to that resource;
- do not replace primary structural selection unless user selects a specific task.

## 12.5 Highlight behavior

Resource highlighting is temporary view state:

```text
normal tasks:
    normal opacity

tasks assigned to highlighted resource:
    full opacity and stronger resource stripe

other tasks:
    slightly reduced opacity
```

Do not hide unrelated tasks by default.

---

# 13. System Builder and component library

## 13.1 Split layout

Use a vertical splitter:

```text
top:
    selected entity editor

bottom:
    component library
```

Both areas remain visible and resizable.

## 13.2 Editor widgets

Use:

- `QFormLayout`;
- `QLineEdit`;
- validated numeric editors;
- `QComboBox`;
- `QTableView`;
- collapsible sections where useful.

Avoid constructing and destroying large widget trees for every minor selection change.

Use a `QStackedWidget` with reusable pages:

```text
System page
Task page
Resource page
Connection page
Empty page
```

## 13.3 Validation

Validation runs through graphics-independent CPSSim code.

Display:

- inline field error;
- section diagnostic;
- global Diagnostics dock entry.

Do not use message boxes for ordinary validation failures.

---

# 14. Canonical Events

Create a `QAbstractTableModel` over `GuiEventTableCache`.

Requirements:

- no `QTableWidget`;
- no `QStandardItem` per event;
- lazy cell conversion;
- stable selection by `EventSequence`;
- search debounce;
- lazy raw JSON;
- cause navigation;
- at least 100,000 rows responsive;
- unchanged generation causes no row rebuild.

Use a filter proxy only if it does not duplicate all row data. Reuse the existing filtered-index cache where practical.

---

# 15. Results and plotting

## 15.1 Results

Results remains completed-run-only.

Reuse immutable `RunResult` and Bosch analysis.

No model rebuild occurs while the run generation remains unchanged.

## 15.2 Native integrated plot

The integrated Qt plot serves operational inspection.

Define a plot canvas abstraction before selecting a library.

Reuse existing downsampled projections.

Benchmark:

- multiple stacked lanes;
- digital transitions;
- one million raw samples;
- nearest point;
- linked cursor;
- interval shading;
- pan/zoom;
- export;
- Qt 6.4;
- license.

## 15.3 External plotter

External Python/PySide6/Matplotlib remains a later goal.

Do not embed Python in the Qt GUI process.

---

# 16. Undo and redo

Use `QUndoStack`.

Commands must mutate CPSSim application/draft state.

Initial commands:

- add task;
- delete task;
- move task;
- edit task;
- add resource;
- delete resource;
- edit resource;
- assign/unassign task;
- create/delete logical connection;
- create/delete communication channel;
- edit route parameters.

Canvas movement commands should merge while dragging, producing one undo step per completed drag.

---

# 17. Testing

## 17.1 Existing tests

All existing tests must continue to pass.

## 17.2 Qt tests

Add Qt Test coverage for:

- Qt bridge run-state changes;
- timer stops while idle;
- bounded Fast continuation;
- QtNodes ID mapping;
- task node resource badge;
- deterministic resource color;
- assignment table model;
- assignment delegate;
- canvas/table selection synchronization;
- node creation at view center;
- occupied-position offset;
- deletion confirmation workflow;
- undo/redo;
- dock persistence;
- event table performance assumptions;
- completed-result publication;
- project replacement and shutdown.

Use:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## 17.3 Manual tests

On Ubuntu 24.04:

- create generic project;
- create Bosch project;
- add/move/delete tasks;
- assign tasks in mapping panel;
- verify node colors and badges;
- select from canvas and table;
- Run/Pause/Reset/Step;
- Fast run to completion;
- inspect 100,000+ events;
- move between different-DPI monitors;
- dock/float/restore panels;
- close with unsaved changes;
- reopen project and verify stable colors and positions;
- compare Bosch conformance outputs;
- verify Dear ImGui build remains functional.

---

# 18. Performance requirements

When Paused or Finished:

- no simulation timer;
- no presentation timer;
- negligible idle CPU;
- no repeated snapshot publication;
- no repeated result finalization;
- no repeated event table rebuild;
- no repeated plot projection rebuild.

During Architecture interaction:

- dragging remains responsive;
- changing assignment updates only affected models;
- no complete simulation snapshot copy;
- resource highlighting does not rebuild domain data;
- node painting does not perform trace scans.

---

# 19. Implementation phases and commits

## Phase 0 — Update design documents

Commit:

```text
goal7: adopt flat resource assignment presentation
```

Deliver:

- this specification;
- DesCartes reference notes;
- Qt/QtNodes ADR;
- updated migration checklist.

## Phase 1 — Extract WorkbenchApplication

Commit:

```text
goal7: extract graphics-independent workbench application
```

Refactor existing ImGui frontend to use it before Qt work continues.

## Phase 2 — Add Qt build and shell

Commit:

```text
goal7: add Qt Widgets workbench shell
```

Deliver:

- build options;
- Qt dependency discovery;
- pinned QtNodes;
- QMainWindow;
- central tabs;
- docks;
- left dock-toggle toolbar;
- layout persistence.

## Phase 3 — Connect simulation lifecycle

Commit:

```text
goal7: connect Qt workbench to simulation lifecycle
```

Deliver:

- bridge;
- actions;
- Live timer;
- Fast continuation;
- background publication;
- status and diagnostics.

## Phase 4 — Architecture prototype

Commit:

```text
goal7: prototype flat QtNodes architecture
```

Deliver:

- task nodes;
- ports;
- logical/communication edges;
- grid;
- zoom;
- pan;
- selection;
- add-at-center;
- stable ID mapping.

Stop and verify the prototype gate.

## Phase 5 — Resource assignment presentation

Commit:

```text
goal7: add resource badges and assignment mapping
```

Deliver:

- deterministic resource colors;
- badges and stripes;
- assignment table;
- editing delegate;
- legend/highlight;
- selection synchronization;
- no containers.

## Phase 6 — System editing workflow

Commit:

```text
goal7: migrate system builder and component library
```

Deliver:

- selected-item editor;
- library;
- creation/deletion;
- validation;
- undo/redo;
- dirty state.

## Phase 7 — Runtime inspection

Commit:

```text
goal7: migrate runtime inspection and canonical events
```

Deliver:

- Inspector;
- Resources;
- Canonical Events;
- diagnostics.

## Phase 8 — Results and visualization

Commit:

```text
goal7: migrate results timeline signals and plotting
```

Deliver:

- Results;
- Timeline;
- Signals;
- native plot prototype/evaluation.

## Phase 9 — Parity and default frontend

Commit:

```text
goal7: verify Qt parity and make Qt the default frontend
```

Do not make Qt the default before parity.

---

# 20. Prototype gate

Before continuing beyond Phase 4, verify:

1. Qt shell launches and closes safely.
2. Existing project/session loads.
3. Run/Pause/Reset/Step work.
4. Architecture renders tasks and connections.
5. Nodes can be added at view center and moved.
6. Selection updates System Builder.
7. No DAG restriction prevents valid CPSSim graphs.
8. Stable ID mapping survives rebuild.
9. Paused/Finished idle CPU is negligible.
10. Different-DPI monitor movement does not crash.
11. Existing Dear ImGui frontend still builds.
12. Bosch canonical outputs remain unchanged.

Before continuing beyond Phase 5, verify:

1. same-resource tasks receive the same accent;
2. different resources remain distinguishable;
3. resource names are visible;
4. unassigned state is explicit;
5. mapping-table edit changes the draft;
6. canvas/table selection is synchronized;
7. project reload preserves color identity and assignments;
8. no resource containers or assignment lines are introduced.

---

# 21. Explicit non-goals

Goal 7 does not:

- change simulation semantics;
- change Bosch timing;
- change event phase ordering;
- change FMI behavior;
- make QtNodes the execution engine;
- require an acyclic architecture;
- implement resource containers;
- implement assignment edges;
- embed Python;
- implement the external Matplotlib application;
- remove Dear ImGui before parity;
- use QtNodes internal serialization as project truth;
- move simulation execution to a worker thread;
- add multi-project ownership to the process.

---

# 22. Completion report

The implementation agent must report:

```text
Baseline commit:
Qt version:
QtNodes repository:
QtNodes commit:
QtNodes license:
Reason for upstream or CPS fork selection:
Build commands:
New targets:
WorkbenchApplication extraction:
Qt bridge design:
Qt event-loop/timer design:
Main-window/dock design:
DesCartes ideas adopted:
DesCartes ideas rejected:
Architecture model:
Stable ID mapping:
Task-node design:
Resource-color algorithm:
Assignment-table design:
Selection synchronization:
Undo/redo:
Canonical Events model:
Plot evaluation:
Layout persistence:
Existing tests:
New tests:
Ubuntu 24.04 manual checks:
Idle CPU:
100,000-event responsiveness:
Multi-DPI result:
Bosch conformance result:
Dear ImGui compatibility:
Known limitations:
Files changed:
Commits:
```
