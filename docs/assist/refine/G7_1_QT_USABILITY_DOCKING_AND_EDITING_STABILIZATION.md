# Goal 7.1 — Qt Usability, Docking, and Editing Stabilization

## 1. Goal

Goal 7.1 corrects the first complete Qt frontend after Goal 7.

The Qt frontend is functionally present, but the current workbench has several shell, docking, editing, and persistence problems:

- Home-page recent-project rows expand across nearly the full window.
- Project panel actions remain conceptually available on Home.
- Theme state is project/workspace-driven rather than a stable application appearance preference.
- A text-only dock selector consumes a permanent left column.
- Dock visibility is repeatedly forced during ordinary state, progress, and theme updates.
- Floating or previously closed docks reappear, especially Results.
- Dock separators are visually weak and constrained by child minimum sizes.
- the System Builder includes a redundant Component Library.
- the Architecture adapter is intentionally read-only for graph creation and connections.
- task resource assignment and task/resource execution profiles are not presented together.
- the bottom analysis area has no explicit collapse/restore workflow.
- a floating bottom dock has no obvious command to return to the bottom tab group.
- Architecture positions do not snap to the same grid that is drawn.

This goal is a GUI correction only. It must not change simulation semantics, tick ordering, Bosch behavior, FMI behavior, canonical events, or completed-run analysis.

---

## 2. Baseline

Repository:

```text
gaochuanchao/CPSSim
```

Reviewed commit:

```text
83cec7f733e4af6ce41728d72c4c33d0412b996e
goal7: verify Qt parity and make Qt the default frontend
```

Keep the existing Qt frontend as the default. Keep the Dear ImGui frontend buildable.

---

## 3. Confirmed root causes

### 3.1 Left-most module selector

`QtMainWindow::build_menus_and_toolbars()` creates a vertical, text-only `dock_toolbar_` and inserts every dock toggle action into it.

This is the permanent column containing:

```text
Experiment Explorer
System Builder
Run Configuration
Runtime Inspector
Resource Assignments
Resources
Canonical Events
Results
Diagnostics
```

Decision:

```text
remove this toolbar
```

Panel visibility remains available through the View menu and normal dock title-bar controls.

### 3.2 Docks repeatedly reopen

`QtMainWindow::synchronize_workbench_chrome()` calls `show_home()` or `show_workbench()` on every application-state/progress/status synchronization.

`show_workbench()` calls `set_workbench_chrome_visible(true)`.

`set_workbench_chrome_visible(true)` calls `setVisible(true)` for every dock.

Consequences:

- a user-closed dock reopens;
- a floating Results dock reappears;
- theme changes can make a floating dock appear like an unrelated popup;
- progress updates can override user panel choices.

Decision:

```text
ordinary synchronization must never force dock visibility
```

Only an actual Home → Workbench transition may initialize or restore workbench layout.

### 3.3 Old floating layouts are restored

The main window restores `state_v1` through `QSettings`. A previously floating Results dock is restored as floating and is then repeatedly shown by the visibility bug.

Decision:

- bump the layout state/key version;
- ignore the existing incompatible/bad state once;
- load geometry/state before enforcing Home visibility;
- ensure Home is the final state after constructor restoration.

### 3.4 Theme is not a global appearance preference

`apply_theme()` reads `bridge.application().workspace().theme`, and selecting a theme changes project workspace state.

Consequences:

- opening another project can change the theme;
- Home appearance depends on the current/default project workspace;
- a theme action emits `workspaceChanged`, which currently triggers the dock visibility bug.

Decision:

```text
Qt theme is a global application preference stored in QSettings
```

Project workspace theme must not override it.

### 3.5 Recent project rows expand excessively

Each recent project is placed in a full-width row. The Open button is added with stretch factor `1` and has no maximum width.

Decision:

- use a centered recent-project container with a bounded width;
- show a short project display name;
- show full path only in secondary text or tooltip;
- use an elided path label;
- keep Remove compact.

### 3.6 Component Library is redundant

The System Builder contains an internal vertical splitter whose lower widget is a Component Library.

Experiment Explorer already has section context menus that call the same component-creation workflow.

Decision:

```text
remove the Component Library and its internal splitter
```

Structural creation is initiated from Experiment Explorer context menus.

### 3.7 Resource Assignments is currently editable in the wrong place

The current `QtResourceAssignmentModel` marks the Resource column editable and installs a combo-box delegate. This lets the user change assignment directly inside the summary table.

That conflicts with the intended interaction model:

```text
Resource Assignments:
    read-only overview and navigation

System Builder:
    authoritative structural editor
```

Decision:

- make every Resource Assignments cell read-only;
- remove the assignment editor delegate;
- retain row selection, sorting, legend filtering/highlighting, and navigation;
- selecting any cell in a task row selects that task;
- System Builder then shows the editable assignment and execution-profile controls.

### 3.8 System Builder task editing is incomplete

The task page contains profile resource and execution controls, but they are visible only when `StructuralSelectionKind::ExecutionProfile` is selected.

Selecting a newly created task hides them.

Task assignment and execution profiles are separate concepts:

```text
task assignment:
    which resource is currently selected for execution

execution profile:
    WCET/execution time for one task-resource pair
```

Decision:

- show current assignment on every selected task page;
- show all task-resource execution profiles on the task page;
- do not require selecting a separate Execution Profile entity to configure WCET;
- all assignment and WCET mutations originate from System Builder, not Resource Assignments.

### 3.9 Architecture graph is intentionally read-only

The current `QtArchitectureGraphModel`:

```cpp
addNode(...)             → InvalidNodeId
connectionPossible(...)  → false
addConnection(...)       → no-op
deleteConnection(...)    → false
deleteNode(...)          → false
```

The current canvas supports:

- viewing;
- selection;
- movement;
- one external Add Task action.

It does not support direct graph connection creation/deletion.

Decision:

- structural entities are created through Explorer;
- Architecture supports selection, movement, snapping, and connection interaction only when backed by a CPSSim domain command;
- do not fake scene-only edits.

---

# Part A — Home and application appearance

## 4. Global appearance preference

Introduce a small Qt-only preference service, for example:

```cpp
class QtAppearancePreferences {
public:
    GuiTheme theme() const;
    void set_theme(GuiTheme);
};
```

Store through:

```text
QSettings("CPSSim", "CPSSim Qt GUI")
key: appearance/theme_v1
values: dark | light
```

Requirements:

1. Load the preference before the Home page is first presented.
2. Apply the palette during `QtMainWindow` construction.
3. Theme remains unchanged across:
   - project creation;
   - project opening;
   - project closing;
   - Home/Workbench transitions.
4. Do not mark a project dirty when the global theme changes.
5. Do not emit project `workspaceChanged` solely for a theme change.
6. QtNodes task/resource colors must use the global current theme.

Remove or stop using `GuiWorkspaceState::theme` as the authoritative theme in the Qt frontend. Legacy ImGui behavior may remain unchanged during this goal.

## 5. Home action policy

While Home is active:

Enabled:

- New Project;
- Open Project;
- Bosch Example;
- recent-project Open/Remove;
- Theme;
- About/Exit where present.

Disabled or hidden:

- Run/Pause/Reset/Step;
- run mode and batch controls;
- Save/Save As/Close Project;
- Undo/Redo;
- Restore Workbench Layout;
- all dock toggle actions;
- all project-panel selection actions.

The View menu may remain visible, but only its Theme submenu should be enabled on Home.

## 6. Home construction order

Current constructor order restores user state after `show_home()`.

Change the order:

```text
build widgets
arrange defaults
load geometry/state
apply global theme
enter Home last
```

Entering Home must hide all project docks and the simulation toolbar after any state restoration.

## 7. Recent projects layout

Create a centered container:

```text
maximum width: 680–760 logical pixels
minimum practical width: 420 logical pixels
```

Recommended row:

```text
[project name]
/shortened/path/to/project.json                         [Open] [Remove]
```

Alternative compact row:

```text
[Open: project name]                                    [Remove]
```

Rules:

- Open button maximum width must be bounded.
- Do not place the complete path in a stretching push button.
- Full path remains in tooltip.
- Elide long path text in the middle.
- Keep row height compact.
- Use consistent spacing between recent entries.
- Empty state remains centered.

---

# Part B — Workbench docking and visibility

## 8. Remove the left dock toolbar

Delete:

```text
dock_toolbar_
toolbar.docks
```

Do not replace it with another permanent column.

Panel access:

- View menu;
- dock close/toggle controls;
- optional keyboard shortcuts for important panels.

## 9. Separate screen transition from chrome synchronization

Replace the current visibility logic with explicit transitions.

Suggested state:

```cpp
enum class QtScreenState {
    Home,
    Workbench
};
```

Functions:

```cpp
void enter_home();
void enter_workbench();
void synchronize_action_state();
```

### `enter_home()`

- switch central stack to Home;
- hide simulation toolbar;
- hide project docks without losing their remembered workbench state;
- disable dock toggle actions;
- preserve global Theme actions;
- do not destroy dock widgets;
- do not rewrite workbench layout.

### `enter_workbench()`

Only execute transition work when the previous screen was Home.

- switch central stack to Workbench;
- show simulation toolbar;
- restore the remembered/default workbench dock state once;
- enable dock toggle actions;
- do not force every dock visible if the user previously closed it.

### `synchronize_action_state()`

May run on every progress/state/status signal.

It may update:

- Run/Pause/Reset/Step enabled state;
- batch controls;
- progress text;
- window modified marker;
- status bar.

It must not call `show()` or `setVisible(true)` on docks.

## 10. Workbench dock visibility memory

Maintain a workbench-only saved state:

```cpp
QByteArray workbench_dock_state_;
```

Before entering Home:

```text
save current workbench state
```

When entering Workbench:

```text
restore saved workbench state
```

If no state exists, apply the default layout.

Do not persist Home-hidden dock visibility as the user's workbench visibility choice.

## 11. Layout migration

Bump:

```cpp
qt_main_window_state_version
```

and QSettings keys:

```text
qt_workbench/geometry_v2
qt_workbench/state_v2
```

Do not load `state_v1`.

The default v2 layout must have:

- Explorer/System Builder left;
- Run Configuration/Runtime Inspector right;
- Resource Assignments/Resources/Canonical Events/Results/Diagnostics tabified at bottom;
- Resource Assignments selected initially;
- Results docked, not floating;
- Results not automatically raised.

## 12. Default dimensions

Initial layout target at 1440×900:

```text
left dock width:           280–340 px
right dock width:          260–320 px
bottom analysis height:    190–240 px
central Architecture:      remaining space, minimum 480×320
```

Use `resizeDocks()` after widgets are installed and after the window is shown or through a zero-delay queued call, because size hints are not reliable before layout activation.

Avoid large child minimum sizes that prevent dock resizing.

## 13. Visible, draggable separators

Apply a centralized style:

```css
QMainWindow::separator {
    background: palette(mid);
    width: 6px;
    height: 6px;
}

QMainWindow::separator:hover {
    background: palette(highlight);
}

QSplitter::handle {
    background: palette(mid);
}

QSplitter::handle:hover {
    background: palette(highlight);
}
```

Requirements:

- visible in light and dark themes;
- 5–7 logical pixel hit target;
- hover feedback;
- no invisible 1-pixel separators.

Set appropriate child policies:

```text
QSizePolicy::Expanding
minimum widths/heights only where necessary
no accidental maximum dimensions
```

## 14. Bottom analysis collapse/restore

QDockWidget does not provide a native “minimize to tab strip” command.

Implement an explicit workbench action:

```text
View → Collapse Bottom Analysis Area
shortcut: Ctrl+J
```

Behavior:

- when expanded, remember the current bottom height and hide/collapse all bottom analysis docks;
- when collapsed, restore the bottom tab group and prior height;
- retain which bottom tab was selected;
- do not float any dock;
- do not change per-dock visible preferences unnecessarily.

Also add a compact chevron/tool button near the central bottom edge or in the status bar.

## 15. Return floating docks to the bottom group

For bottom analysis docks, add an explicit action:

```text
Dock in Bottom Analysis Group
```

Available through:

- dock title-bar context menu;
- View menu submenu;
- optional custom title-bar button.

Implementation:

```cpp
addDockWidget(Qt::BottomDockWidgetArea, dock);
tabifyDockWidget(bottom_anchor, dock);
dock->setFloating(false);
dock->raise();
```

Do not automatically force a deliberately side-docked panel back to bottom. Use the explicit action.

Set sensible `allowedAreas` for each dock.

---

# Part C — Results popup defect

## 16. Results must never open from unrelated state changes

Fix the visibility root cause in Part B.

Additional invariants:

- theme change does not show Results;
- progress change does not show Results;
- status change does not show Results;
- completed-result publication updates Results content but does not show, raise, float, or focus the Results dock;
- closing Results keeps it closed until the user reopens it;
- floating Results remains floating only if the user deliberately left it so and the state was saved;
- default layout always docks Results in the bottom group.

Do not call:

```cpp
results_dock->show();
results_dock->raise();
results_dock->setFloating(true);
```

from result refresh/publication paths.

---

# Part D — Explorer-driven structural editing

## 17. Remove Component Library

Simplify `QtSystemBuilderWidget`:

```text
QScrollArea
└── reusable QStackedWidget editor pages
    ├── System
    ├── Resource
    ├── Task
    ├── Connection
    └── Empty
```

Remove:

- component-library label;
- component `QListWidget`;
- Add Selected Component button;
- Delete Selected Entity button;
- the internal System Builder vertical splitter.

Deletion and creation move to Explorer context menus.

The System Builder remains a property editor, not a palette.

## 18. Explorer structure

Simplify the primary tree:

```text
Project
├── Resources
├── Tasks
└── Connections
```

Execution profiles are task properties, not a required top-level navigation section.

Under Connections, distinguish:

```text
Logical Dependencies
Communication Channels
```

If changing the persisted domain model for logical dependencies is outside scope, show only the kinds that are currently representable and document the limitation.

## 19. Explorer context menus

### Resources section

```text
Add Resource
```

### Tasks section

```text
Add Task
```

### Connections section

```text
Add Communication Channel...
Add Logical Dependency...   only when backed by domain persistence
```

### Resource item

```text
Rename
Duplicate
Delete...
```

### Task item

```text
Rename
Duplicate
Delete...
Add Execution Profile...
Create Connection From...
```

### Connection item

```text
Edit
Delete...
```

Every action must use existing CPSSim application/draft workflows and `QUndoStack`.

After creation:

- select the new entity;
- reveal its editor;
- for a task, place its node near the current Architecture view center;
- mark the project dirty;
- validate.

Use explicit action names, not the generic word `Add`.

---

# Part E — Architecture editing and grid behavior

## 20. Clarify Architecture responsibility

The Architecture canvas supports:

- view;
- select;
- drag;
- grid snap;
- zoom/pan/fit;
- connection selection;
- connection creation only when backed by a CPSSim command;
- delete through a domain command.

Structural entity creation starts in Explorer.

Remove the redundant `Add Task` toolbar action after Explorer task creation is complete.

Recommended Architecture toolbar:

```text
Fit
100%
Auto Layout
Snap to Grid [checked]
```

## 21. Place Explorer-created tasks in the canvas

Extend component creation to accept optional presentation placement:

```cpp
create_component(
    StructuralSection section,
    std::optional<GuiLayoutPoint> requested_position
);
```

The Explorer obtains the current Architecture view center through a small callback/service.

Flow:

```text
Explorer → Add Task
    → create draft task and stable ID
    → assign node position near view center
    → offset if occupied
    → validate
    → rebuild graph
    → select and reveal node
```

Do not infer a new ID by comparing labels.

## 22. Grid drawing and snapping

Use one shared constant:

```cpp
inline constexpr qreal architecture_grid_step = 20.0;
inline constexpr int architecture_major_grid_every = 5;
```

The drawn fine grid and position snap must use the same value.

Node movement:

```text
drag freely or with live snap
release
    → round x/y to nearest 20 logical pixels
    → update scene item
    → persist snapped logical position
```

Default and auto-layout positions must also be multiples of the grid step.

The grid origin must remain stable under pan and zoom.

## 23. Domain-backed connection editing

The current model is read-only.

Do not merely change `connectionPossible()` to `true`.

First define application commands:

```cpp
CreateLogicalConnection
CreateCommunicationChannel
DeleteConnection
```

Validation must check:

- source and destination exist;
- no invalid self-loop where prohibited;
- duplicate relation policy;
- project edit policy;
- Bosch-protected structure;
- network-route requirements;
- logical versus communication semantics.

Then adapt QtNodes callbacks to these commands.

If logical dependencies do not yet have project persistence, keep logical creation disabled and explicitly show the reason.

---

# Part F — Complete task editing

## 24. Resource Assignments is a read-only overview

`QtResourceAssignmentsWidget` must be a navigation and status panel only.

Remove:

- `Qt::ItemIsEditable` from the Resource column;
- `QtResourceAssignmentDelegate`;
- combo-box creation and `setData()` assignment mutation;
- edit triggers such as double-click or keyboard editing.

Set:

```cpp
table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
table_->setSelectionBehavior(QAbstractItemView::SelectRows);
table_->setSelectionMode(QAbstractItemView::SingleSelection);
```

Selection behavior:

```text
click any cell in a row
    → select the whole row
    → StructuralSelection selects that TaskId
    → Architecture selects/reveals the task node
    → System Builder opens the Task page
```

The row remains visibly highlighted while the task is selected from:

- Resource Assignments;
- Architecture;
- Experiment Explorer;
- System Builder-related navigation.

The panel may still provide:

- sorting;
- resource color legend;
- temporary resource highlighting;
- task/resource/WCET/status display;
- tooltips explaining invalid assignment or missing profile.

It must not own any editable widgets or mutate `DraftTaskAssignment` or execution profiles.

## 25. Task page layout

For every selected task, show:

### Identity and timing

```text
Task ID
Name
Period
Deadline
Offset
Priority
```

### Current assignment

```text
Assigned resource: [Unassigned | CPU0 | CPU1 | ...]
Assignment status: Valid / Missing profile / Inaccessible
```

### Execution profiles

A compact table:

```text
Resource     Execution time/WCET     Accessible     Status
CPU0         10 ticks                Yes            Valid
CPU1         —                       No             No profile
```

Use `QAbstractTableModel`, not `QTableWidget`.

## 26. Editing behavior

Changing Assigned resource:

- updates `DraftTaskAssignment`;
- does not silently invent an execution time;
- selects the matching profile row;
- shows `Missing execution profile` when WCET is absent.

Editing a WCET cell:

- creates or updates `DraftExecutionProfileKey{task, resource}`;
- validates;
- updates Resource Assignments;
- updates the Architecture badge/tooltip;
- supports undo/redo.

Deleting a profile:

- requires confirmation if currently assigned;
- either blocks deletion or leaves assignment visibly invalid;
- never silently reassigns the task.

## 27. New task workflow

After Add Task:

1. task is created and selected;
2. Task page is visible;
3. user can immediately set timing and priority;
4. user chooses Assigned resource;
5. user enters WCET for that resource;
6. validation changes from incomplete to valid;
7. Architecture node updates its resource stripe and badge;
8. Resource Assignments row updates.

Do not require the user to select a separate Execution Profile tree item.

---

# Part G — Tests

## 28. Main-window tests

Add tests for:

1. Home disables all dock toggle actions.
2. Home keeps Theme actions enabled.
3. Home hides simulation toolbar and project docks after state restoration.
4. Recent project row/button has bounded width.
5. Global theme persists across project open/close.
6. Project workspace cannot override Qt global theme.
7. Theme change does not show a hidden/floating Results dock.
8. Progress signal does not reopen any closed dock.
9. Status signal does not reopen any closed dock.
10. Completed result does not show/raise Results.
11. Workbench transition restores prior dock visibility once.
12. v1 layout is not loaded as v2.
13. default Results dock is tabified at bottom.
14. left dock toolbar does not exist.
15. Collapse/restore bottom analysis retains height and selected tab.
16. Dock in Bottom Analysis Group re-tabifies a floating dock.

## 29. System Builder, Explorer, and Resource Assignments tests

1. Component Library does not exist.
2. System Builder has no internal library splitter.
3. Right-click Resources section exposes Add Resource.
4. Right-click Tasks section exposes Add Task.
5. Add Task selects the new task.
6. Selected task always shows assignment controls.
7. Selected task always shows execution profile table.
8. Resource Assignments exposes no editable item flags.
9. Resource Assignments installs no editor delegate.
10. Double-clicking or pressing an edit key in Resource Assignments does not open an editor.
11. Clicking any Resource Assignments cell selects the full task row.
12. Selecting a row selects the corresponding task and opens its System Builder Task page.
13. Selecting the same task from Architecture or Explorer highlights the matching assignment row.
14. Assignment change in System Builder updates draft assignment and the read-only row.
15. WCET edit in System Builder creates/updates the execution profile and the read-only row.
16. undo/redo restores both assignment and profile.
17. deleting the current profile produces explicit validation behavior.

## 30. Architecture tests

1. Explorer-created task is placed near view center.
2. occupied position is offset deterministically.
3. saved node positions are grid multiples.
4. moved node snaps to grid on release.
5. grid origin remains aligned after pan/zoom.
6. scene-only node creation remains prohibited.
7. connection creation is enabled only through supported domain commands.
8. Bosch-protected graph remains non-editable where required.

## 31. Manual verification

On Ubuntu 24.04:

- launch with no project;
- inspect bounded recent-project layout;
- confirm only theme-related View action is available on Home;
- switch dark/light and open/close multiple projects;
- confirm theme remains stable;
- verify no popup or floating dock appears;
- open generic project;
- confirm no left module-selector column;
- resize left/right/bottom separators over a wide range;
- collapse and restore bottom area;
- float Resource Assignments and explicitly dock it back into bottom tabs;
- close Results, change theme, run, pause, and finish; confirm Results stays closed;
- add Resource and Task through Explorer context menus;
- select task and configure timing, assignment, and WCET;
- verify node appears near canvas center;
- move node and verify grid snap;
- save/reload and verify positions and assignments;
- run Bosch conformance and compare canonical output.

---

# Part H — Implementation order

## Commit 1

```text
goal7.1: fix Home theme and panel availability
```

- global Qt theme preference;
- Home action policy;
- recent-project bounded layout;
- constructor ordering.

## Commit 2

```text
goal7.1: stabilize dock visibility and layout restoration
```

- remove left dock toolbar;
- transition-only visibility;
- state v2;
- Results popup fix;
- default sizes.

## Commit 3

```text
goal7.1: add visible separators and bottom panel controls
```

- separator styling;
- collapse/restore;
- explicit re-dock action.

## Commit 4

```text
goal7.1: move structural creation into Experiment Explorer
```

- remove Component Library;
- richer context menus;
- domain-backed create/delete/duplicate.

## Commit 5

```text
goal7.1: make assignment overview read only and complete task editing
```

- remove editing from Resource Assignments;
- preserve row selection and synchronized navigation;
- task assignment field in System Builder;
- execution-profile table in System Builder;
- validation and undo/redo.

## Commit 6

```text
goal7.1: align Architecture editing with the visible grid
```

- center placement;
- snap;
- grid constants;
- supported connection commands.

## Commit 7

```text
goal7.1: verify Qt workbench usability stabilization
```

- tests;
- documentation;
- manual Ubuntu checks;
- conformance report.

---

## 32. Completion report

```text
Baseline commit:
Home recent-project layout:
Global theme persistence:
Home action policy:
Removed dock toolbar:
Dock visibility state machine:
Layout state version:
Results popup root cause and fix:
Default dock dimensions:
Separator styling:
Bottom collapse/restore:
Floating dock re-dock:
Component Library removal:
Resource Assignments read-only enforcement:
Resource row/selection synchronization:
Explorer context menus:
New task workflow:
Task assignment model:
Execution profile editor:
Architecture center placement:
Grid/snap constant:
Connection editing scope:
New tests:
Existing tests:
Ubuntu 24.04 manual checks:
Multi-DPI result:
Bosch conformance result:
Known limitations:
Files changed:
Commits:
```
