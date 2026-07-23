# Step 6 — Center View and Initial Graph Centering

## Objective

Add a "Center View" command that centers the Architecture graph at the current
zoom, and automatically center the graph once when a project is opened.

## Implementation

### Part 1 — Center View QAction

One new `QAction` owned by `QtArchitectureView`:

- Object name: `action.architecture.centerView`
- Text: "Center View"
- Tooltip: "Center the graph without changing zoom"
- No keyboard shortcut

Added to the Architecture toolbar between Fit All and 100%:

    [Fit All] [Center View] [100%] [Auto Layout] [Snap to Grid] | [Add Task] | [link type]

Also added to all context menus (empty canvas, task node, connection) alongside Fit All.

### Part 2 — Centering implementation

Two new private methods:

- `std::optional<QRectF> graph_node_bounds() const` — iterates `model_.allNodeIds()`,
  gets each `scene_->nodeGraphicsObject(node_id)`, unions `sceneBoundingRect()`.

- `void center_graph_in_view()` — calls `view_->centerOn(bounds.center())`.
  Does nothing when there are no task nodes.

### Part 3 — Zoom preservation

`center_graph_in_view()` calls only `view_->centerOn()`, which changes the
scroll position but not the `QTransform`. The zoom level is preserved.

### Part 4 — Automatic centering after project open

`refresh()` detects project root changes by comparing
`bridge_.application().active_project().root()` with a stored
`observed_project_root_`. When the root changes:

1. `observed_project_root_` is updated.
2. `schedule_initial_center()` is called.
3. A `QTimer::singleShot(0, ...)` queues the centering after the next event
   loop iteration, ensuring the graphics objects and viewport are ready.

Ordinary refresh events (selection, draft, appearance, workspace) do NOT
trigger centering because the project root hasn't changed.

### Part 5 — Hidden Architecture tab

When the Architecture view is not visible at project-open time, the centering
is deferred via `showEvent`. The `initial_center_pending_` flag remains false
after the timer fires if the view is hidden, and the `showEvent` handles it
on the first show.

### Part 6 — Project close

When `observed_project_root_` transitions from a value to `nullopt` (project
closed), the stale identity is cleared and `initial_center_pending_` is reset.

### Files changed

- `apps/qt_gui/architecture_view.hpp` — new members, methods, showEvent
- `apps/qt_gui/architecture_view.cpp` — implementation
- `tests/qt_gui/architecture_model_test.cpp` — 11 new tests

### Tests added

1. `centerViewAction_exists` — action exists with correct text
2. `centerViewAction_enabled_with_nodes` — enabled when graph has nodes
3. `centerViewAction_disabled_without_nodes` — disabled on empty graph
4. `centerViewAction_preserves_zoom` — QTransform unchanged after center
5. `centerViewAction_does_not_mutate` — no selection, undo, or dirty changes
6. `centerViewAction_centers_on_node_bounds` — viewport center ≈ node bounds center
7. `centerViewAction_empty_graph_does_not_crash` — safe with no nodes
8. `centerViewAction_available_while_running` — viewing command, not structural
9. `centerViewAction_available_for_bosch` — available in Bosch projects too
10. `initialCentering_after_project_open` — graph centered after project creation
11. `initialCentering_not_on_ordinary_refresh` — selection/appearance refresh does not recenter
12. `initialCentering_hidden_tab` — centering works when shown after project open
