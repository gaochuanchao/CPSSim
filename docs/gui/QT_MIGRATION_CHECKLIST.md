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
- [x] Qt bridge provides bounded Live/Fast progression, no idle timer, queued
      finalizer publication, and clean cancellation/shutdown.
- [x] Flat QtNodes prototype renders tasks and semantic connections, accepts
      cycles, synchronizes structural selection, adds at center with stable
      occupied offset, and preserves strong identities across rebuilds.
- [x] Assignment gate shows a stable accent and resource-name badge (including
      `Unassigned`), uses a read-only `QAbstractTableModel`, synchronizes table
      and canvas selection, and has no resource containers or assignment edges.
- [x] Explorer context actions and the scrollable System Builder mutate the
      CPSSim draft through commands and `QUndoStack`, never through scene-only
      state; assignment and WCET editing belongs to the selected Task page.
- [x] Explorer, run configuration, runtime inspector, resources, events, and
      diagnostics retain their Goal 6 ownership and selection behavior.
- [x] Results, timeline, signals, and the integrated native plot use immutable
      shared completed/presentation data; a permanent plotting dependency is
      not selected without a separate benchmark/license/Qt ADR.
- [x] Qt Test runs with `QT_QPA_PLATFORM=offscreen`, including 100,000-event
      model responsiveness coverage.
- [x] Existing tests, Bosch conformance, CLI, exports, workspace/project
      persistence, and Dear ImGui build pass unchanged.
- [x] Ubuntu 24.04 uses system Qt 6.4.2; bridge tests prove no Paused/Finished
      simulation timer, and existing display-scale tests cover non-cumulative
      scale changes. Physical mixed-DPI motion remains a documented desktop
      check because the verification environment is offscreen.

Qt is the default `cpssim_gui`; Dear ImGui remains the independently buildable
`cpssim_imgui_gui`. Dear ImGui removal is explicitly outside Goal 7.
