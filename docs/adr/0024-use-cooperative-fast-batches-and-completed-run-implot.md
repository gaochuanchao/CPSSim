# ADR-0024: Use Cooperative Fast Batches and Completed-Run ImPlot

- Status: Accepted
- Date: 2026-07-20
- Owners: CPSSim maintainers
- Related goal: Goal 5

## Context

One event tick per rendered frame unnecessarily couples long GUI runs to the
display rate. Copying complete traces and rebuilding result analysis during a
run is also expensive and gives a partial run the appearance of a final result.

## Decision

`SimulationController` remains single-threaded and processes Live one-step
updates or cooperative Fast batches. Fast batches stop at an event-transition
budget, an integer-tick target, the finished state, or an internal 25 ms
responsiveness budget. Wall-clock time controls only when the controller
returns to the GUI; it never becomes simulation input.

`SimulationProgress` is a cheap tick/state/event-count value. During Fast
Running, heavy views retain the last detached complete presentation snapshot.
Pause, Finish, Reset, switching to Live, and explicit coherent operations
publish a complete snapshot.

Each reconstructed runtime has a generation. `CompletedRunResultCache` accepts
only a Finished snapshot and builds one immutable `RunResult` per generation.
Reset and semantic/project replacement invalidate it. Paused and Running
states cannot construct final metrics or visualizer series.

The GUI pins [ImPlot v1.0](https://github.com/epezent/implot) at commit
`524f9fcd48d76c13fdf94c5ffbba8787a1ff7e39` under its MIT license. Only
`implot.cpp` and `implot_items.cpp` are built; demo sources are excluded.
ImPlot context lifetime follows ImGui, and startup requires the existing
OpenGL backend's vertex-offset support. Graphics-independent signal search,
unit lanes, range resolution, exact tick conversion, and source-preserving
downsampling remain CPSSim-owned.

## Consequences

- Live and Fast use the same `step_to_next_event()` semantic operation.
- GUI responsiveness needs no worker thread or synchronization.
- Results, export, and plots share one completed immutable source.
- Plot interaction changes selection only and cannot alter simulation state.
- Full-resolution signals remain authoritative; downsampling is drawing-only.
- Matplot++ and publication image export remain future work.

## Validation

Tests compare Live, Fast-Events, and Fast-Ticks final outputs; exercise progress
and batch validation; prove finish-only one-build cache behavior; and validate
signal search, unit lanes, ranges, unit conversion, and source-preserving
downsampling. Full GUI, DPI, Bosch, canonical-event, export, CLI, sanitizer,
and clang-tidy verification remains required.
