# ADR-0018: Use a Single-Threaded Snapshot and Command GUI Boundary

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related task: T17

## Context

T17 adds a minimal Dear ImGui visualization without allowing rendering speed,
window events, or widgets to alter deterministic simulation semantics. The GUI
needs run, pause, reset, and next-event controls plus resource and trace data.
It must not inspect live mutable kernel containers or introduce graphics types
into `cpssim_core`.

`SimulationEngine::run()` was previously one indivisible operation. A GUI
could only remain responsive by placing that call on another thread or by
bypassing the engine to manipulate its event queue. The first choice adds
synchronization and lifetime complexity before it is needed; the second breaks
the public-state boundary.

## Decision

### Shared stepping mechanism

`SimulationEngine` exposes `step_to_next_event()`. One call initializes the
experiment when necessary and processes every deterministic phase at exactly
one next logical event tick. `run()` loops over the same operation. A logical
tick remains atomic to presentation code: the GUI cannot pause between
completion, release, scheduling, and caused-action phases at that tick.

`finished()` reports that no in-horizon event tick remains. Calls to the step
operation after completion return false. Reset is not an engine mutation: the
presentation controller destroys the old runtime and constructs a new one from
stored immutable configuration and task assignments.

### GUI support boundary

The separately linked `cpssim_gui_support` library depends on `cpssim_core` and
contains no Dear ImGui, GLFW, or OpenGL types. It owns:

- a FIFO of `GuiCommand` values;
- copied immutable experiment input and allocation results;
- the fixed-priority policy and current engine used by the minimal GUI; and
- construction of detached `SimulationSnapshot` values.

The snapshot copies current tick, control state, canonical events, and public
resource/Ready views. Rendering receives that value as read-only input. A
snapshot can become old, but it cannot become a dangling view or mutate the
engine.

### Single-threaded application loop

T17 uses one application thread. Widgets enqueue commands during a rendered
frame. At the next update boundary the controller applies commands in FIFO
order. If state remains `Running`, it processes one complete logical event
tick. The number of wall-clock frames required to finish may vary, but the
selected event, event phases, sequence values, and resulting trace do not.

No mutex, worker thread, lock-free queue, or real-time pacing is introduced.
If later experiments make one event tick too expensive for responsive display,
a new ADR must define synchronization and snapshot publication ownership.

### Optional graphics dependency

`CPSSIM_BUILD_GUI` is off by default. Enabling it adds a separate `cpssim_gui`
executable using Dear ImGui v1.92.8 with its official GLFW and OpenGL3
backends. The archive URL and SHA-256 are pinned. Headless core, CLI, tests,
Bosch adapter, and FMI adapter do not link these libraries or require a display
server.

The first task-resource profile for each task is used only as the example GUI
application's deterministic placement input. `ConfiguredResourceAllocator`
still validates and applies it before releases; the GUI does not choose a
resource for individual jobs.

## Consequences

Positive:

- headless and GUI execution share one event-processing implementation;
- window refresh timing cannot reorder or partially apply a logical tick;
- the renderer cannot mutate scheduler, resource, queue, or trace state;
- reset starts from a clean ownership graph;
- GUI-independent command and snapshot tests need no window server; and
- headless builds retain no graphics dependency.

Limiting:

- continuous Run advances at most one event tick per rendered frame;
- only the fixed-priority policy and a simple application-selected assignment
  are exposed by the first GUI;
- snapshots copy the complete event log, which favors clarity over large-trace
  performance;
- there is no background worker, playback-speed control, tick stepping,
  timeline, or functional-signal plot; and
- GUI availability currently requires GLFW/OpenGL development packages plus a
  first-time Dear ImGui download.

## Alternatives considered

### Run the engine on a worker thread

Deferred because T17 does not yet have a measured responsiveness need that
justifies synchronization, shutdown, exception-transfer, and snapshot-
publication complexity.

### Let the GUI read Scheduler and EventQueue directly

Rejected because live views may become invalid during mutation and would make
presentation code depend on kernel ownership details.

### Implement a second event loop in the GUI controller

Rejected because duplicated phase logic could diverge from headless `run()`.

### Link Dear ImGui into `cpssim_core`

Rejected because the simulator must remain usable and testable without a GUI.

## Validation

- stepping processes all phases at one event tick;
- FIFO commands cover pause, run, reset, and next-event behavior;
- a retained snapshot does not change after the controller advances;
- GUI-controller and direct headless runs serialize identical canonical trace
  bytes;
- normal builds and tests succeed with `CPSSIM_BUILD_GUI=OFF`; and
- the optional executable builds with the pinned Dear ImGui GLFW/OpenGL3
  integration and can execute `--help` without a display server.
