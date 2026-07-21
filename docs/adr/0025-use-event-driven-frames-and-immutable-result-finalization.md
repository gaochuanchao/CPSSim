# ADR-0025: Use Event-Driven Frames and Immutable Result Finalization

- Status: Accepted
- Date: 2026-07-21

## Context

The Goal 5 GUI could continue rebuilding immediate-mode frames and heavy
post-run presentation on the GUI thread after semantic data stopped changing.
Dear ImGui does not support retained dirty rectangles, and simulation/FMI state
must remain single-threaded. Post-run metric and plot derivation nevertheless
operates entirely on a detached completed snapshot.

## Decision

The native loop classifies each iteration as Running, Interactive,
BackgroundPending, or FullyIdle before beginning a Dear ImGui frame. Those
states select `glfwPollEvents`, `glfwWaitEventsTimeout(0.05)`, or
`glfwWaitEvents`. A redraw generation plus high-value pointer regions from the
last completed frame determines whether a normal complete frame is needed.
CPSSim does not implement dirty rectangles or retained widgets.

Snapshot publication uses independent runtime, simulation-data, and
presentation generations. Live publication is limited to 15 Hz; coherent
semantic boundaries publish immediately. One immutable shared
`CompletedRunData` is detached on Finish.

A managed `std::jthread` may derive signals, generic metrics, Bosch analysis,
and immutable indexes only from that data. It cannot access mutable
controller/session/FMU state or GUI/workspace/selection state. Completion wakes
GLFW with `glfwPostEmptyEvent`; Ready is adopted only by the GUI thread at a
frame boundary. Runtime replacement and shutdown cancel and join the worker.

Event and plot projections are generation-keyed graphics-independent caches.
The canonical source order and full-resolution completed data remain
authoritative.

## Consequences

- Fully idle Paused/Finished windows can block indefinitely without creating
  frames or periodic expensive work.
- Live and Fast simulation still execute through the same GUI-thread engine
  path; result threading cannot change canonical behavior.
- Completed trace/observation storage is shared rather than copied into each
  result consumer.
- Pointer geometry is one rendered frame old and therefore needs invalidation
  plus one conservative recovery frame after layout changes.
- Shutdown and semantic replacement must explicitly cancel and join the
  finalizer before graphics/runtime owners disappear.

This decision refines, but does not supersede,
[ADR-0018](0018-use-a-single-threaded-snapshot-command-gui-boundary.md) and
[ADR-0024](0024-use-cooperative-fast-batches-and-completed-run-implot.md).
