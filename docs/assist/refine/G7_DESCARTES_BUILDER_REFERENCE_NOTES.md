# DesCartes Builder Reference Notes for CPSSim Goal 7

Reference repository:

```text
https://github.com/CPS-research-group/descartes-builder/
```

Observed implementation patterns:

- C++17 Qt application.
- Qt 6 Widgets and OpenGL.
- QtNodes submodule at `app/external/qtnodes`.
- The submodule points to `CPS-research-group/nodeeditor`.
- Central `GraphicsView` hosted in tabbed graph documents.
- `CustomGraph` derives from `QtNodes::DirectedAcyclicGraphModel`.
- `DagGraphicsScene` owns node-scene interaction.
- `QMainWindow` provides menu, toolbar, central tabs, side docks, and bottom log.
- A left vertical toolbar toggles sidebar docks.
- The Blocks panel combines selected-node editing and a component library.
- Clicking a library item creates a node near the current view center.
- Creation offsets the position when another node already occupies it.
- Selection drives the property editor and chart availability.
- Graph projects are saved through QtNodes scene serialization and packed data.

Ideas to adopt:

1. Qt Widgets desktop shell.
2. Central QtNodes `GraphicsView`.
3. Dock-toggle toolbar.
4. Selected-item editor plus component library.
5. Add-at-center workflow.
6. Stable menu actions and shortcuts.
7. Dirty/modified state.
8. Bottom diagnostics/output panel.
9. Centralized graph/node/connection styling.

Ideas not to adopt directly:

1. Directed-acyclic graph assumption.
2. QtNodes graph as authoritative CPSSim domain state.
3. Direct editor mutation of graph-owned node objects.
4. Global selected-node variable.
5. `QTableWidget` for large CPSSim data.
6. QtNodes internal scene file as CPSSim project truth.
7. Resource-container presentation.

CPSSim-specific replacement:

```text
DesCartes block graph
    → CPSSim flat task graph

DAG model
    → AbstractGraphModel

graph-owned node data
    → EditableSystemDraft and project model

resource containers
    → resource stripe + badge + assignment table

scene serialization
    → project.json + workspace.json

global selected node
    → StructuralSelection
```

The DesCartes implementation is a practical reference for interaction design and Qt integration, but CPSSim retains its existing deterministic simulation and application boundaries.
