# Qt Workbench Migration Checklist

This is the durable Goal 7 gate checklist.  It supplements
[ADR-0026](../adr/0026-use-qt-widgets-and-flat-qtnodes-adapter.md); it does not
change simulation semantics or project formats.

## Fixed dependency decision

- Qt compatibility baseline: system Qt 6.4, components Core, Gui, Widgets,
  Test, and OpenGLWidgets.
- QtNodes: upstream `paceholder/nodeeditor`, commit
  `1b173f885b52e4fd9616f663ea288435ccf1d0d8`, BSD-3-Clause.
- DesCartes fork evaluated: `CPS-research-group/nodeeditor`, commit
  `7c44af0f2e59105ee87766628c7e2d2a32a922a1`.
- QtNodes examples, tests, and docs are disabled in CPSSim builds.
- Dear ImGui remains independently buildable until parity is verified.

## Phase gates

- [x] Graphics-independent `WorkbenchApplication` owns lifecycle, publication,
      results, edits, selection, workspace, exports, and diagnostics.
- [x] Qt shell launches with stable central tabs, docks, actions, object names,
      and versioned geometry/state restoration.
- [ ] Qt bridge provides bounded Live/Fast progression, no idle timer, queued
      finalizer publication, and clean cancellation/shutdown.
- [ ] Flat QtNodes prototype renders tasks and semantic connections, accepts
      cycles, synchronizes structural selection, adds at center with stable
      occupied offset, and preserves strong identities across rebuilds.
- [ ] Assignment gate shows a stable accent and resource-name badge (including
      `Unassigned`), edits through a `QAbstractTableModel`, synchronizes table
      and canvas selection, and has no resource containers or assignment edges.
- [ ] System Builder/component library mutate the CPSSim draft through commands
      and `QUndoStack`, never through scene-only state.
- [ ] Explorer, run configuration, runtime inspector, resources, events, and
      diagnostics retain their Goal 6 ownership and selection behavior.
- [ ] Results, timeline, signals, and the integrated native plot use immutable
      shared completed/presentation data; a permanent plotting dependency is
      not selected without a separate benchmark/license/Qt ADR.
- [ ] Qt Test runs with `QT_QPA_PLATFORM=offscreen`, including 100,000-event
      model responsiveness coverage.
- [ ] Existing tests, Bosch conformance, CLI, exports, workspace/project
      persistence, and Dear ImGui build pass unchanged.
- [ ] Ubuntu 24.04 Qt 6.4, idle CPU, native multi-DPI, and desktop workflows are
      recorded before Qt becomes the default frontend.

Qt becomes the default only after every applicable parity gate is complete.
Dear ImGui removal is explicitly outside Goal 7.
