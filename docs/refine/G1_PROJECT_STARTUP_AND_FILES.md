# G1 — Project Startup and File Management

## Goal

Allow CPSSim to start with no experiment loaded and provide three clear entry points:

```text
Create New Project
Open Existing Project
Bosch Challenge Example
```

Also provide proper file/directory selection and a dedicated default project folder.

## Current limitation

The GUI currently creates one mandatory `GuiSimulationSession` at startup using the default example configuration. General experiment opening is disabled, and run-plan paths are entered manually.

## Target user workflow

### Home screen

When no project is open, show:

```text
CPSSim
[Create New Project]
[Open Existing Project]
[Bosch Challenge Example]

Recent Projects
```

### Active project

After successful creation or loading, switch to the existing workbench.

### File menu

When a project is open:

```text
New Project...
Open Project...
Open Recent
Save Project
Save Project As...
Close Project
Exit
```

Run-plan import/export may remain separate.

## Project directory convention

Use a dedicated default folder:

```text
projects/
└── <project-name>/
    ├── project.json
    ├── system.json
    ├── run-plans/
    ├── workspace.json
    └── results/
```

Add `projects/` to `.gitignore`.

Do not save new user files to the repository root by default.

## Project file responsibilities

### `project.json`

Contains project metadata and relative file references only.

Suggested fields:

```json
{
  "schema_version": 1,
  "name": "bosch-v10-shared-cloud",
  "system_file": "system.json",
  "workspace_file": "workspace.json",
  "default_run_plan": "run-plans/default.json",
  "scenario": {
    "kind": "bosch"
  }
}
```

### `system.json`

Existing validated experiment/system configuration.

### `run-plans/*.json`

Existing run-plan format.

### `workspace.json`

Presentation-only state:

- theme;
- visible panels;
- splitter positions;
- selected analysis tab;
- selected signals.

Workspace state must not affect run-plan signatures or simulation behavior.

## Required architecture change

Replace mandatory session ownership with optional project/session ownership.

Conceptual structure:

```text
GuiApplication
├── Home state
└── Workbench state
    ├── ProjectContext
    └── GuiSimulationSession
```

A new project/session must be constructed fully before replacing the current one.

Failure rule:

```text
load → validate → construct → replace
```

If loading fails, retain the currently open project.

## Native file and directory selection

Add a pinned lightweight file-dialog dependency behind `CPSSIM_BUILD_GUI`, or use a platform-neutral implementation already approved by the project.

Required selectors:

- open project file;
- choose new project directory;
- save project as;
- open run plan;
- save run plan;
- choose export directory.

Do not use a plain path text box as the primary workflow. A text field may remain for manual editing beside a `Browse...` button.

## Bosch Challenge wizard

The home-screen Bosch action opens a wizard.

### Step 1 — Trajectory

```text
10 m/s
12.5 m/s
15 m/s
Custom trajectory directory
```

### Step 2 — Scenario

```text
Dedicated
Shared cloud
Custom run plan
```

### Step 3 — Horizon

```text
Complete trajectory
Custom stop tick
```

### Step 4 — Project location

```text
Project name
Parent directory
[Browse...]
```

### Step 5 — Review

Display all selected values before creation.

The wizard must create an interactive paused GUI session. Do not call the existing Bosch run service that executes the whole simulation immediately.

Introduce a Bosch project/session factory that returns the inputs required by `GuiSimulationSession`:

```text
ExperimentConfig
RunPlan
FunctionalModelFactory
SignalRegistry
Project metadata
```

Keep Bosch-specific construction outside generic GUI support.

## Suggested task split

### G1.1 — Optional session and home screen

Inspect first:

```text
apps/gui/main.cpp
apps/gui/gui_application.*
src/cpssim/gui/simulation_session.*
```

Acceptance:

- GUI opens without loading `basic.json`;
- home screen is visible;
- current workbench still functions when a session is supplied;
- no simulation semantics change.

Implemented boundary:

- application startup owns an optional `GuiSimulationSession`;
- no command-line configuration selects Home, while a supplied configuration
  selects the existing Workbench;
- Home exposes placeholder actions only; project persistence, native dialogs,
  and the Bosch wizard remain later G1 tasks;
- session replacement accepts a fully constructed owned session before changing
  the active application state, and clearing returns safely to Home.

### G1.2 — Project model and directory convention

Add GUI/application-level project types and strict JSON persistence.

Acceptance:

- a project can be saved and reopened;
- all internal paths are relative to the project directory;
- malformed files fail before replacing the active project.

### G1.3 — File dialogs

Acceptance:

- open/save actions use dialogs;
- cancel leaves state unchanged;
- selected paths are displayed clearly;
- default location is `projects/`.

### G1.4 — Bosch project wizard

Inspect first:

```text
src/cpssim/application/bosch_run_service.*
src/cpssim/bosch/
apps/cli/commands/bosch_run_command.*
```

Acceptance:

- all three supplied trajectories are selectable;
- dedicated/shared-cloud are selectable;
- created session opens paused;
- bundled FMU path is resolved automatically;
- invalid selections show a local error without closing the wizard.

### G1.5 — Recent projects and workspace loading

Limit recent-project history to a small fixed number. Missing projects should be removable from the list.

## Tests

Add headless tests for:

- project JSON round trip;
- relative-path resolution;
- invalid project files;
- atomic replacement behavior;
- Bosch wizard request validation;
- cancellation behavior for dialog abstraction;
- workspace state not changing run-plan/system signatures.

Manual GUI checks:

- first launch;
- create/open/close project;
- cancel every dialog;
- reopen recent project;
- move window across monitors after project load.

## Non-goals

- full system editing;
- multi-vehicle Bosch;
- new schedulers;
- results export;
- direct graph editing.
