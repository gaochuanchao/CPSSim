# ADR-0026: Use Qt Widgets and a Flat QtNodes Presentation Adapter

- Status: Accepted
- Date: 2026-07-22
- Related goal: Goal 7

## Context

The Dear ImGui workbench proves the simulator/application boundary, but its
large editable workbench now needs native docking, reusable item models,
accessible forms, and a graph editor with conventional selection and undo
behavior.  The frontend change must preserve the single-threaded simulation
boundary and the immutable completed-run finalizer established by ADR-0018 and
ADR-0025.

DesCartes Builder demonstrates a useful Qt main-window shell and component
workflow.  Its directed-acyclic, graph-owned data model is not suitable for
CPSSim: control architectures may contain cycles, and project/domain state
must remain authoritative outside the scene.

## Decision

CPSSim adds a C++ Qt 6 Widgets frontend with a Qt 6.4 compatibility baseline.
The graphics-independent `WorkbenchApplication` owns project/session
lifecycle, publications, completed results, drafts, selections, workspace,
exports, and diagnostics.  Both frontends adapt that owner; neither calls the
simulation engine directly.  Dear ImGui remains buildable until verified Qt
parity.

The Architecture view is a flat graph:

- every task is an independent node;
- logical and communication connections are the only graph edges;
- resources are not node containers;
- task/resource assignment is not drawn as an edge;
- each task shows a deterministic resource accent and a textual resource
  badge, including an explicit `Unassigned` state; and
- assignments are edited in a docked model/view table.

QtNodes is a GUI-only editor/view adapter.  CPSSim uses
`QtNodes::AbstractGraphModel`, `QtNodes::BasicGraphicsScene`, and
`QtNodes::GraphicsView`; it does not use `DirectedAcyclicGraphModel` or treat
`DataFlowGraphModel` as simulator semantics.  `EditableSystemDraft`, the run
plan, project files, and workspace files remain authoritative.  QtNodes scene
serialization is not a CPSSim persistence format.

QtNodes' `NodeId` is a 32-bit unsigned integer.  The adapter therefore owns
explicit bidirectional maps between QtNodes IDs and strong CPSSim graph IDs.
It never casts or truncates a CPSSim identity.

The selected dependency is upstream
[`paceholder/nodeeditor`](https://github.com/paceholder/nodeeditor) at immutable
commit `1b173f885b52e4fd9616f663ea288435ccf1d0d8`, licensed BSD-3-Clause.  Its
generic graph/scene/view API, undo stack, grid, custom painters, and current
copy/paste fixes are sufficient.  The DesCartes fork
`CPS-research-group/nodeeditor` at
`7c44af0f2e59105ee87766628c7e2d2a32a922a1` retains DesCartes-specific DAG
classes and is behind upstream generic scene/group/copy fixes.  Those fork-only
DAG facilities are deliberately unnecessary.  Dependency examples, tests,
and documentation are disabled in normal CPSSim builds.

Qt owns native idle waiting.  Live execution uses a bounded timer around
16 ms; Fast execution schedules one bounded continuation at a time.  No timer
runs while Paused, Finished, or NotConfigured.  The existing 15 Hz complete
presentation publication limit stays in `WorkbenchApplication`.  Immutable
worker completion is adopted on the Qt GUI thread using a queued invocation.

`QMainWindow::saveGeometry` and versioned `saveState` bytes are presentation
preferences only.  CPSSim project/workspace values continue to own semantic
and architecture-position state.  Stable `objectName` values identify docks.

## Consequences

- `cpssim_core` remains free of Qt, Dear ImGui, and QtNodes.
- Qt widgets can share all Goal 6 generations, caches, finalization, selection,
  validation, and export behavior with the legacy frontend.
- Cycles are allowed and graph appearance cannot become domain truth.
- Resource assignment remains readable without relying on color.
- Qt and QtNodes become optional GUI dependencies; Ubuntu packages provide Qt,
  while QtNodes is pinned by commit.
- Two frontends must be kept compatible until the explicit parity gate makes
  Qt the default and schedules Dear ImGui removal in a later goal.

## Alternatives considered

Using the DesCartes DAG model was rejected because feedback cycles are valid.
Using `DataFlowGraphModel` was rejected because execution/data propagation is
owned by CPSSim.  Resource containers and assignment edges were rejected
because they make tasks appear nested and clutter the semantic connections.
PySide6 or embedded Python was rejected because the production process is C++.
Removing Dear ImGui immediately was rejected because the migration requires a
working parity reference.

## Validation

The migration checklist in
[`../gui/QT_MIGRATION_CHECKLIST.md`](../gui/QT_MIGRATION_CHECKLIST.md) defines
the prototype, assignment, parity, performance, Qt 6.4, offscreen-test,
multi-DPI, and Bosch-conformance gates.
