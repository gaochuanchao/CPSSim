# Goal 6 — GUI Performance, Architecture Interaction, and Post-Run Analysis Stabilization

## 1. Goal

Goal 6 stabilizes CPSSim after the Goal 5 GUI refinement.

The current Bosch workflow is functionally complete, but long simulations leave the GUI with high CPU usage, slow Canonical Events interaction, a lagging Plot Visualizer, and occasional operating-system “not responding” warnings. The Architecture, Results, and System Builder layouts also require additional interaction and sizing refinement.

This goal must solve those problems without changing simulation semantics.

The implementation must preserve:

- task release, start, preemption, resume, completion, and deadline semantics;
- canonical event ordering;
- fixed-priority scheduling behavior;
- resource accounting;
- network send/delivery semantics;
- Bosch trigger generation;
- FMI stepping behavior;
- Bosch conformance outputs;
- Live/Fast deterministic equivalence;
- project and workspace persistence;
- CLI behavior;
- result export formats;
- multi-DPI stability.

The performance work is the release gate. Architecture and layout refinements are not considered complete until the post-run GUI remains responsive and idle CPU usage is no longer close to one fully occupied core.

---

# 2. Fixed semantic decisions

These decisions are already made. Do not redesign them.

## 2.1 Tick-boundary semantics

CPSSim uses integer ticks as time boundaries.

For a task released at tick `r`:

```text
the job becomes ready at the beginning of tick r
```

For a job that starts at tick `s` and executes continuously for `C` ticks:

```text
execution interval = [s, s + C)
finish tick        = s + C
```

The job output becomes available at the beginning of `finish_tick`.

For relative deadline `D`:

```text
absolute deadline = r + D
```

The job is on time when:

```text
finish_tick <= absolute_deadline
```

A completion exactly at the deadline is valid.

At the same tick, completion/write semantics precede downstream reads and deadline checking. The current phase precedence must remain:

```text
ExecutionCompletion
MessageDelivery
DeadlineCheck
JobRelease
PolicyUpdate
Scheduling
CausedAction
```

Do not change the kernel timing model in Goal 6.

## 2.2 Bosch one-tick internal handoff

The pinned Bosch reference currently uses an internal one-tick separation between task completion and the network-send trigger.

That internal timing exists to preserve exact Bosch/MATLAB conformance. It is not a user-visible communication delay.

The GUI must show:

```text
Sensor → Estimator
Communication latency: 80 ticks
```

It must not show:

```text
81 ticks communication latency
send offset
post-completion offset
```

The internal one-tick Bosch handoff remains adapter-owned and hidden.

Do not:

- add it to the displayed latency;
- make it a normal editable GUI field;
- apply it to logical connections;
- change generic task-completion semantics because of it.

## 2.3 Connection semantics

The GUI presents two connection kinds through one interaction model:

```text
Logical connection
Communication channel
```

A logical connection has:

```text
displayed communication latency = 0 ticks
no MessageSend event
no MessageDelivery event
```

A communication channel has:

```text
displayed communication latency = configured transmission delay
MessageSend and MessageDelivery events according to the network model
```

Logical connections must not be converted into zero-delay `MessageRouteSpec` values.

Internally, logical and communication connections remain distinct.

## 2.4 Dear ImGui update model

CPSSim does not implement partial framebuffer repainting.

The GUI uses:

```text
event-driven complete frames
+ cached panel data
+ visible-item clipping
```

When fully idle, CPSSim does not call `ImGui::NewFrame()`, rebuild views, render OpenGL draw data, or swap buffers.

When any visible state changes, CPSSim renders one normal complete Dear ImGui frame.

## 2.5 Results lifecycle

Results is a completed-run view.

Do not rebuild final Results while Running or Paused.

When the run finishes:

1. detach one immutable completed-run dataset;
2. publish the final workbench snapshot;
3. finalize metrics and signal indexes;
4. publish one completed result;
5. reuse that completed result until the run generation changes.

Reset, Apply and restart, project replacement, and runtime replacement invalidate the completed result.

---

# 3. Current performance problems to eliminate

The current design has several expensive frame-level operations.

## 3.1 Snapshot churn

The GUI currently requests complete snapshots too often. A complete snapshot copies:

- the canonical event trace;
- functional observations;
- resource state;
- experiment presentation.

After a run reaches `Finished`, no new simulation data exists. Recopying the complete snapshot every frame is prohibited.

## 3.2 Duplicate completed-run ownership

The current flow can retain:

- runtime-owned trace data;
- a presentation-snapshot copy;
- another snapshot stored by `RunResult`;
- derived signal data.

Goal 6 must replace avoidable by-value duplication with shared immutable ownership.

## 3.3 Canonical Events reconstruction

Canonical Events must not rebuild all rows, JSON strings, search text, sorting, and filtering every frame.

The ImGui clipper alone is insufficient because it limits drawing but not model reconstruction.

## 3.4 Plot reconstruction

The Plot Visualizer must not:

- derive Bosch analysis every frame;
- rebuild plot lanes every frame;
- downsample unchanged signals every frame;
- allocate fresh X/Y arrays every frame;
- recreate all interval data every frame.

## 3.5 Busy idle loop

When Paused or Finished and no interaction is occurring, the native GUI loop must not spin continuously at maximum CPU usage.

## 3.6 Synchronous completion stall

Completed-result finalization must not block the GUI long enough for the operating system to mark the application as unresponsive.

---

# 4. Required implementation architecture

Use the following architecture. Do not substitute a materially different design without documenting a blocking technical reason.

---

# Part A — Performance instrumentation

## 5. Development-only GUI profiler

Add a lightweight profiler enabled in Debug builds or through a compile-time flag such as:

```text
CPSSIM_GUI_PROFILING
```

Provide a small optional window:

```text
View → GUI Performance
```

The profiler must measure at least:

- session update time;
- simulation batch time;
- progress update time;
- complete snapshot publication time;
- completed-result finalization enqueue time;
- completed-result worker time;
- architecture model/layout time;
- Canonical Events cache-build time;
- Canonical Events filter time;
- Results render time;
- plot cache-build time;
- plot render time;
- total CPU frame time.

Track counters:

- complete snapshot publication count;
- completed-result build count;
- event-row cache rebuild count;
- event-filter projection rebuild count;
- plot cache rebuild count;
- Bosch-analysis rebuild count;
- architecture-layout rebuild count;
- rendered frames;
- poll calls;
- timed-wait calls;
- indefinite-wait calls;
- skipped Dear ImGui frame count;
- passive mouse-move suppression count;
- BoundarySensitive enter/leave redraw count;
- PositionSensitive mouse-move redraw count;
- discrete-input redraw count;
- pointer-region map rebuild count;
- background wake-up count.

Display data size estimates:

- canonical event count;
- functional observation count;
- selected plot point count;
- detached completed-run memory estimate where practical.

The profiler must not allocate heavily or distort Release behavior.

## 6. Performance counter invariants

After a run reaches `Finished` and the user performs no interaction for five seconds:

- snapshot publication count remains constant;
- completed-result build count remains constant;
- event-row cache rebuild count remains constant;
- event-filter projection count remains constant;
- plot cache rebuild count remains constant;
- Bosch-analysis rebuild count remains constant.

Add automated tests for these invalidation rules where possible.

---

# Part B — Generation-based presentation publication

## 7. Add explicit generations

Introduce stable generation counters.

Suggested state:

```cpp
using GuiGeneration = std::uint64_t;

struct PresentationGenerationState {
    GuiGeneration runtime_generation{};
    GuiGeneration simulation_data_generation{};
    GuiGeneration presentation_generation{};
};
```

Required meaning:

- `runtime_generation` changes when a controller/runtime is created or reset.
- `simulation_data_generation` changes when simulation-visible data advances.
- `presentation_generation` changes only when a new complete presentation snapshot is published.

Do not use event count alone as a generation because multiple changes may occur with the same count.

## 8. Snapshot publication policy

Implement one central function deciding whether to publish a complete presentation snapshot.

A complete snapshot is published only when one of these conditions is true:

- no presentation snapshot exists;
- a Live-mode refresh deadline is reached and simulation data changed;
- Fast mode reaches Pause;
- the run reaches Finished;
- Reset occurs;
- mode switches from Fast to Live;
- Apply and restart creates a new runtime;
- project/session replacement occurs;
- an explicit command requests a coherent snapshot.

After `Finished`, the final presentation snapshot is immutable and must not be recopied on later frames.

## 9. Live-mode refresh rate

Live mode may advance simulation more frequently than it publishes presentation data.

Use a presentation refresh interval:

```text
default maximum refresh rate: 15 Hz
```

The implementation should use `steady_clock`.

At each GUI frame:

```text
advance the controller according to Live semantics
update lightweight progress
publish a full snapshot only if:
    data changed
    and 1/15 second has elapsed since the last publication
```

Always publish immediately on:

- Pause;
- Finish;
- Reset;
- explicit Step completion;
- mode transition requiring coherence.

Do not tie simulation stepping to the 15 Hz presentation rate.

Make the refresh rate an internal constant in Goal 6. Do not add another user preference yet.

## 10. Avoid session update when idle

Do not call controller update work when all are true:

- state is Paused or Finished;
- command queue is empty;
- no transition is pending;
- no explicit coherent snapshot was requested.

Expose a cheap query such as:

```cpp
bool GuiSimulationSession::needs_update() const;
```

or an equivalent controller-level query.

The query must not scan traces.

---

# Part C — Event-driven Dear ImGui frame scheduling

## 11. Rendering decision

Dear ImGui is immediate-mode. CPSSim must not attempt framebuffer dirty-rectangle repainting.

The correct design is:

```text
no application/UI change
    → do not start a Dear ImGui frame

application/UI change
    → rebuild and render one complete Dear ImGui frame
```

A rendered frame may describe the whole visible interface, but unchanged panel data must be reused from caches.

Do not implement:

- partial framebuffer clearing;
- retained ImGui widget trees;
- manual dirty rectangles;
- reuse of old ImGui draw lists as mutable UI state;
- continuous rendering merely to preserve the visible window.

The previously presented framebuffer remains visible while CPSSim waits.

## 12. Frame activity and wait strategy

Create pure/testable decisions.

Suggested enums:

```cpp
enum class GuiFrameActivity {
    Running,
    Interactive,
    BackgroundPending,
    FullyIdle
};

enum class GuiWaitStrategy {
    Poll,
    WaitWithTimeout,
    WaitIndefinitely
};
```

Classify as `Running` when:

- the simulation state is Running;
- Fast-mode progress is changing;
- a continuous animation is intentionally active.

Classify as `Interactive` when:

- a mouse button is held;
- an ImGui item is active;
- a splitter is being dragged;
- an Architecture node/resource is being moved or resized;
- the Plot Visualizer is being panned or zoomed;
- a text field is active;
- a popup/menu is open;
- a window is being resized or moved;
- a command was issued and its resulting state has not yet been rendered.

Classify as `BackgroundPending` when:

- completed-result finalization is running;
- another managed background operation may publish new GUI-visible state;
- no direct user interaction is active.

Classify as `FullyIdle` when:

- simulation state is Paused, Finished, or NotConfigured;
- no ImGui interaction is active;
- no popup/menu/drag/resize is active;
- no background result is pending;
- no redraw has been requested.

Map activity to waiting:

```text
Running:
    Poll

Interactive:
    Poll

BackgroundPending:
    WaitWithTimeout

FullyIdle:
    WaitIndefinitely
```

## 13. GLFW event strategy

Use:

```cpp
switch (wait_strategy) {
case GuiWaitStrategy::Poll:
    glfwPollEvents();
    break;

case GuiWaitStrategy::WaitWithTimeout:
    glfwWaitEventsTimeout(0.05);
    break;

case GuiWaitStrategy::WaitIndefinitely:
    glfwWaitEvents();
    break;
}
```

Use the timeout only while a background operation may complete without a GLFW input event.

For a fully idle Paused/Finished GUI:

```text
glfwWaitEvents()
```

is required.

Do not wake at 20 Hz merely to rebuild the same frame.

Retain VSync, but do not depend on VSync as the CPU limiter.

When swap blocking is unreliable, cap Poll-mode rendering to approximately 60 Hz using `steady_clock` sleep/yield logic.

Do not insert additional sleeps inside a Fast simulation batch.

## 14. Background wake-up

When the completed-result worker publishes Ready, Failed, or Cancelled state, it must wake a thread blocked in `glfwWaitEvents()`.

Use:

```cpp
glfwPostEmptyEvent();
```

through a safe notification callback/bridge.

The worker may request a wake-up, but it must not:

- call ImGui;
- render;
- mutate GUI selection;
- mutate workspace state;
- publish a partially constructed result.

The GUI thread consumes the completed publication and renders the next frame.

Ensure the wake-up callback cannot outlive GLFW/application shutdown.

## 15. Redraw invalidation

Maintain a cheap `redraw_requested` flag or generation.

Set it when:

- user input changes hover/active state;
- progress changes;
- a snapshot is published;
- finalization status changes;
- the completed result becomes available;
- workspace settings change;
- selection changes;
- tabs/panels/splitters change;
- a dialog/window opens or closes;
- event filters change;
- plot settings or selected signals change;
- DPI, framebuffer size, exposure, restore, or window size changes.

Consume the redraw request only after a frame is successfully presented.

## 16. Skip the complete Dear ImGui frame when idle

The main loop must be structured so that `NewFrame`, view construction, `ImGui::Render`, OpenGL submission, and `glfwSwapBuffers` are skipped when no redraw is required.

Conceptual loop:

```cpp
while (!glfwWindowShouldClose(window)) {
    const auto activity = determine_frame_activity();
    const auto wait_strategy = determine_wait_strategy(activity);

    wait_for_events(wait_strategy);

    collect_input_and_background_notifications();

    if (!redraw_requested() && determine_frame_activity() == GuiFrameActivity::FullyIdle) {
        continue;
    }

    update_session_only_if_needed();
    begin_imgui_frame();
    draw_application();
    render_and_present();
    acknowledge_redraw();
}
```

Do not call `ImGui::NewFrame()` and then decide not to render.

Do not rebuild view models before determining that a frame is required.

## 17. Partial computation, not partial framebuffer repaint

When one frame is required, issue a normal complete Dear ImGui frame, but rebuild only affected data models.

Required cache behavior:

```text
Architecture:
    reuse graph/layout unless system, assignment, or layout generation changes

Canonical Events:
    reuse rows and projected indices unless generation/filter changes

Results:
    reuse immutable completed result

Plot Visualizer:
    reuse cached X/Y and Bosch analysis unless cache key changes

Resources/Timeline/Signals:
    reuse the latest published presentation snapshot
```

Use `ImGuiListClipper` and child clipping for large visible collections.

A frame caused only by mouse hover must not:

- copy the simulation trace;
- rebuild event rows;
- derive Bosch metrics;
- rebuild plot vectors;
- rerun Architecture auto-layout.

## 18. Optional texture caching is deferred

Do not introduce off-screen texture caching for Architecture or plots in Goal 6 unless profiling proves that cached view models and event-driven frames are insufficient.

Texture caching would complicate:

- hit testing;
- ports and drag operations;
- DPI transitions;
- selection overlays;
- resizing.

The default Goal 6 solution is event-driven frames plus cached panel models.


## 19. Pointer-event redraw policy

Use a deliberately simple pointer policy. Do not build a complete retained-mode interaction system for every Dear ImGui widget.

Default behavior:

```text
mouse movement alone
    → no redraw

mouse click/release
    → redraw one frame

mouse wheel
    → redraw one frame

keyboard/text input
    → redraw one frame

movement inside a position-sensitive region
    → redraw interaction frames

active drag/pan/resize
    → continuous paced frames
```

Mouse movement over passive content must not trigger a Dear ImGui frame.

Passive content includes:

- empty panel background;
- static labels;
- completed Results text;
- ordinary table cells without hover-dependent behavior;
- unused whitespace;
- non-interactive borders;
- empty Architecture canvas in Select mode.

## 20. Pointer-region classes

Track only high-value regions.

Suggested enum:

```cpp
enum class GuiPointerRegionKind {
    Passive,
    BoundarySensitive,
    PositionSensitive,
    DragHandle
};
```

Suggested record:

```cpp
struct GuiPointerRegion {
    ImRect bounds;
    GuiPointerRegionKind kind;
    std::uint64_t stable_id;
};
```

### Passive

Cursor movement never requests redraw.

Examples:

- static text;
- empty panel area;
- inactive whitespace;
- ordinary completed-run metrics.

### BoundarySensitive

Redraw only when the cursor enters or leaves the region.

Examples:

- ordinary button;
- tab;
- task box;
- resource box;
- selectable row;
- static connection corridor;
- input/output port in Select mode.

Movement within the same boundary-sensitive region does not request another frame.

### PositionSensitive

Exact cursor position affects rendering or interaction.

Examples:

- Plot Visualizer canvas;
- Scheduling Timeline hover cursor;
- Architecture edge when nearest-edge or coordinate-dependent tooltip is shown;
- range-selection canvas;
- slider while active.

Cursor movement inside a position-sensitive region requests redraw.

### DragHandle

Cursor movement requests redraw while active or hovered where cursor feedback is required.

Examples:

- splitter;
- scrollbar thumb;
- resource resize handle;
- Architecture move/resize handles.

## 21. Pointer-region registration scope

Initially register only high-value CPSSim regions:

- Plot Visualizer canvases;
- Scheduling Timeline;
- Architecture tasks;
- Architecture resources;
- Architecture connection hit corridors;
- Architecture input/output ports;
- Architecture resize handles;
- center/sidebar/Results splitters;
- custom scrollbars;
- major toolbar controls where hover feedback is important.

Do not wrap every Dear ImGui control in Goal 6.

Ordinary System Builder controls may initially be click-driven:

```text
cursor moves across input/button
    → no frame required

mouse click
    → render one frame
    → Dear ImGui resolves focus/click
```

## 22. Discrete input events

Always request redraw for:

- mouse-button press;
- mouse-button release;
- mouse wheel;
- keyboard press/release;
- text input;
- tab/window focus change;
- resize;
- restore/expose;
- framebuffer-size change;
- DPI/content-scale change;
- project/session replacement;
- completed-result publication;
- simulation progress publication.

Do not first perform an expensive widget-level hit test for click or wheel events.

The cost of one frame for a discrete input event is acceptable.

## 23. Mouse-move decision

Use the last rendered frame's pointer-region map.

Conceptual callback:

```cpp
void on_cursor_moved(double x, double y) {
    latest_mouse_position = {x, y};

    if (active_drag_or_resize) {
        request_redraw();
        return;
    }

    const auto hit = pointer_regions.hit_test(latest_mouse_position);

    if (hit.kind == GuiPointerRegionKind::PositionSensitive ||
        hit.kind == GuiPointerRegionKind::DragHandle) {
        request_redraw();
        return;
    }

    const auto new_hovered_id =
        hit.kind == GuiPointerRegionKind::BoundarySensitive
            ? std::optional{hit.stable_id}
            : std::nullopt;

    if (new_hovered_id != previously_hovered_region) {
        previously_hovered_region = new_hovered_id;
        request_redraw();
    }
}
```

Movement over passive content updates the stored cursor position but does not request a frame.

## 24. Region-map lifetime

The cached pointer-region map is valid only for the exact rendered layout.

Invalidate it after:

- window resize;
- framebuffer resize;
- DPI/content-scale change;
- font rebuild;
- tab change;
- panel visibility change;
- splitter movement;
- scrolling;
- Architecture layout change;
- Architecture zoom/pan change;
- popup/menu opening or closing;
- project/session replacement;
- any layout generation change.

After invalidation:

1. request one complete frame;
2. rebuild the pointer-region map during that frame;
3. publish the map only after successful rendering.

Never use stale rectangles after a DPI or layout change.

## 25. Input boxes and caret behavior

Input boxes do not require redraw on ordinary cursor motion.

Required behavior:

```text
cursor passes over input box
    → no frame, unless explicitly tracked as BoundarySensitive

mouse clicks input box
    → one frame, focus resolved

keyboard/text input
    → one frame per input event

focused but inactive input
    → no frame required
```

Caret blinking may use a timed wake-up only while a text field is focused.

A simpler acceptable Goal 6 behavior is to disable or suspend caret blinking while fully idle.

Do not keep the whole GUI rendering continuously solely for caret blinking.

## 26. Scrolling behavior

Mouse movement over a scrollable child does not request redraw.

Required behavior:

```text
wheel event
    → one frame
    → apply scroll
    → rebuild visible interaction regions

scrollbar press/drag
    → interactive continuous frames until release
```

Canonical Events must not redraw merely because the cursor moves over the table.

## 27. Architecture pointer policy

Use:

```text
empty canvas in Select mode:
    Passive

task/resource box:
    BoundarySensitive

connection hit corridor:
    BoundarySensitive by default

connection hit corridor with coordinate-dependent tooltip:
    PositionSensitive

input/output port in Select mode:
    BoundarySensitive

canvas in Arrange/Assign drag:
    PositionSensitive

resource resize handle:
    DragHandle
```

Connection hit corridors should be wider than the visible stroke:

```text
recommended hit tolerance: 6–10 logical pixels
```

Do not require pixel-perfect line hovering.

## 28. Plot and Timeline pointer policy

Plot Visualizer canvas is PositionSensitive because:

- nearest-sample tooltip depends on X position;
- cursor tick depends on X position;
- click selection depends on plot coordinates;
- pan/zoom requires continuous feedback.

Scheduling Timeline is PositionSensitive only when hover tick, nearest interval, or range selection is enabled.

Static plot controls outside the canvas may remain click-driven or BoundarySensitive.

## 29. Fallback behavior

When pointer-region state is unknown, invalid, or inconsistent:

```text
request one conservative complete frame
```

Do not risk missing input because of stale or unavailable region metadata.

Unknown state must not cause continuous rendering. After one recovery frame, return to the normal policy.

## 30. Pointer-policy acceptance criteria

In a fully idle Finished state:

- moving the cursor over static Results text does not render;
- moving over empty panel background does not render;
- moving within the same ordinary task/resource box does not repeatedly render;
- entering or leaving a tracked BoundarySensitive item renders one frame;
- clicking anywhere renders one frame;
- wheel input renders one frame;
- moving within Plot Visualizer renders as needed;
- Architecture drag/resize renders continuously until release;
- pointer-region invalidation after DPI/layout changes forces one safe rebuild;
- no expensive model cache is rebuilt merely because of passive mouse movement.

---

# Part D — Immutable completed-run data and background finalization

## 26. Shared completed data

Introduce one immutable detached completed dataset.

Suggested design:

```cpp
struct CompletedRunData {
    SimulationSnapshot snapshot;
    GuiGeneration run_generation;
};

using CompletedRunDataPtr = std::shared_ptr<const CompletedRunData>;
```

`RunResult` must not own another full by-value copy of the snapshot.

Change it toward:

```cpp
struct RunResult {
    std::string scenario_kind;
    CompletedRunDataPtr data;
    GuiSignalBuildResult signals;
    RunMetrics metrics;
    std::optional<BoschResultAnalysis> bosch_analysis;
};
```

Access the final snapshot through:

```cpp
result.data->snapshot
```

The final presentation, Results, Plot Visualizer, Canonical Events cache, and exporters should share immutable data wherever their lifetime permits.

## 27. Finalization worker

Implement a managed completed-result finalizer using `std::jthread`.

Do not run the simulation engine on this worker.

The worker receives only:

- `CompletedRunDataPtr`;
- scenario kind;
- immutable performance summary;
- cancellation token.

The worker may perform:

- signal-model construction;
- generic metric derivation;
- Bosch result analysis;
- immutable plot-series indexing;
- immutable event-row metadata preparation if beneficial.

The worker must never access:

- mutable controller state;
- mutable FMU state;
- ImGui;
- GLFW;
- application-owned mutable selections;
- workspace state.

## 28. Finalizer lifecycle

Suggested component:

```cpp
class CompletedRunFinalizer {
public:
    void request(FinalizationRequest request);
    void cancel_and_join();
    std::optional<CompletedRunResult> take_published();
    FinalizationStatus status() const;
};
```

Required states:

```text
Idle
Finalizing
Ready
Cancelled
Failed
```

On `Finished`:

1. publish/detach the final immutable completed dataset once;
2. request finalization;
3. immediately return to GUI rendering;
4. Results displays `Finalizing completed run...`;
5. when worker finishes, publish the result at a GUI-thread frame boundary.

On:

- Reset;
- Apply and restart;
- project close;
- project replacement;
- application shutdown;

call `cancel_and_join()` before destroying referenced state.

Use either:

- a mutex-protected result slot; or
- atomic/shared-pointer publication where supported.

Do not publish partially constructed results.

## 29. Failure behavior

If finalization throws:

- capture a diagnostic string;
- return the finalizer to `Failed`;
- keep the application responsive;
- retain the completed snapshot;
- disable Plot Visualizer and completed-result export;
- show a recoverable error in Results.

Do not terminate the GUI process.

## 30. Exactly-once finalization

For each run generation:

- one finalization request;
- at most one completed result;
- repeated Finished frames do not enqueue more work.

Add a generation check at request and publication boundaries.

---

# Part E — Canonical Events cache

## 31. Cache model

Move event-table projection out of the frame render path.

Suggested state:

```cpp
struct EventRowCacheEntry {
    Tick tick;
    EventSequence sequence;
    EventType type;
    EventPhase phase;
    EventEntityRefs entities;
    std::string compact_search_text_lower;
};

struct EventViewCache {
    GuiGeneration presentation_generation{};
    std::vector<EventRowCacheEntry> rows;
    std::vector<std::size_t> filtered_indices;
    GuiEventFilters applied_filters;
    bool rows_valid{};
    bool projection_valid{};
    std::optional<std::size_t> selected_row;
    std::optional<std::string> selected_raw_json;
};
```

## 32. Row construction

Rebuild `rows` only when `presentation_generation` changes.

During row construction:

- preserve the canonical trace order;
- do not stable-sort unless an invariant test proves the source order is not canonical;
- store IDs and enums as structured data;
- build one compact lowercase search blob per row;
- do not serialize raw JSON for every row.

The search blob should include:

- tick;
- sequence;
- event type;
- phase;
- task ID;
- job ID;
- resource ID;
- message ID;
- vehicle ID where present.

Avoid repeated temporary string concatenation during filtering.

## 33. Filtering

Rebuild `filtered_indices` only when:

- row cache changes;
- event type filter changes;
- ID filter changes;
- text filter changes.

Debounce free-text filtering:

```text
150 ms after the most recent text edit
```

Exact ID/type filter changes may apply immediately.

Filtering returns row indices, not row copies.

## 34. Raw JSON

Generate raw JSON lazily only when:

- a row is selected and its detail pane requests JSON;
- the user invokes Copy JSON;
- export explicitly requires it.

Cache selected-row JSON until selection or generation changes.

## 35. Rendering

Continue using `ImGuiListClipper`, but clip the `filtered_indices` vector.

Do not allocate per visible row where avoidable.

Keep selection stable by event sequence rather than visible-row position.

## 36. Large-trace behavior

For a completed Bosch run:

- opening Canonical Events may build the cache once;
- scrolling must not rebuild rows;
- changing column visibility must not rebuild rows;
- selecting a row may build only its raw JSON;
- opening another tab and returning must reuse the cache;
- idle frames must perform no event projection work.

---

# Part F — Plot Visualizer cache

## 37. Immutable Bosch analysis

Compute `BoschResultAnalysis` once during completed-result finalization.

Store it in the completed result.

Do not call `derive_bosch_result_analysis()` during every visualizer frame.

## 38. Plot cache key

Use a stable cache key containing:

```cpp
struct PlotCacheKey {
    GuiGeneration run_generation;
    std::vector<GuiSignalId> selected_signals;
    GuiPlotXAxisUnit x_axis_unit;
    Tick range_begin;
    Tick range_end;
    int plot_pixel_width;
    bool markers;
};
```

Y-axis limits, legend visibility, grid visibility, and line thickness do not require rebuilding X/Y data.

## 39. Plot cache value

Suggested structure:

```cpp
struct CachedPlotSeries {
    GuiSignalId id;
    std::string display_name;
    std::string unit;
    bool digital;
    std::vector<double> x;
    std::vector<double> y;
};

struct PlotDataCache {
    std::optional<PlotCacheKey> key;
    std::vector<GuiPlotLane> lanes;
    std::vector<CachedPlotSeries> series;
    std::vector<BoschCriticalIntervalView> visible_critical_intervals;
};
```

The cache is rebuilt only when the key changes.

## 40. Downsampling algorithm

Use drawing-only bucket min/max downsampling.

Point budget:

```text
budget = clamp(2 × plot pixel width, 512, 8192)
```

For each visible source series:

1. binary-search the full-resolution sample range;
2. preserve the first and last sample;
3. divide the range into buckets;
4. retain bucket minimum and maximum in original order;
5. retain exact digital transitions for Boolean/digital signals;
6. do not modify the source series.

The nearest-sample tooltip continues to binary-search the full-resolution original series.

Do not use the downsampled values for export or metrics.

## 41. Cache invalidation

Rebuild plot data when:

- completed run changes;
- selected signal list changes;
- selected signal order changes;
- X-axis unit changes;
- visible range changes;
- plot pixel width changes materially;
- marker mode changes only if the cached representation depends on it.

Do not rebuild for:

- mouse hover;
- cursor movement;
- legend toggle;
- grid toggle;
- line thickness;
- selected-tick movement;
- window movement without width change.

## 42. Signal browser caching

Cache search results until:

- search string changes;
- completed signal model changes.

Do not rescan all descriptors every frame.

## 43. Critical intervals

Filter Bosch critical intervals to the visible tick range during cache build.

Avoid emitting one expensive shaded ImPlot item per off-screen interval.

When interval count is very large, merge adjacent/overlapping intervals before rendering.

## 44. Visible lanes

Render only lanes currently visible within the Plot Visualizer child area.

Use one vertical scrolling child for lanes and `ImGuiListClipper` or an equivalent lane-visibility calculation.

## 45. Interaction

Plot cursor movement updates shared selected tick without rebuilding plot data.

The plot window remains non-modal.

The Plot Visualizer must remain unavailable until completed finalization reaches `Ready`.

---

# Part G — Architecture connection model

## 46. Unified presentation type

Add a GUI connection abstraction.

Suggested design:

```cpp
enum class GuiConnectionKind {
    Logical,
    Communication
};

struct GuiConnectionPresentation {
    GuiConnectionId id;
    GuiConnectionKind kind;
    TaskId source_task_id;
    TaskId destination_task_id;
    Tick displayed_latency;
    bool editable;
    bool protected_endpoints;
};
```

Required values:

```text
Bosch logical edge:
    kind = Logical
    displayed_latency = 0

Bosch uplink/downlink:
    kind = Communication
    displayed_latency = 80
```

The hidden Bosch one-tick adapter timing is not included.

## 47. Builder content for connections

System Builder uses one `Connection` editor.

Logical connection:

```text
Connection
Source: Estimator
Destination: Controller
Type: Logical
Communication latency: 0 ticks
No network send/delivery events are generated.
```

Bosch communication channel:

```text
Connection
Source: Sensor
Destination: Estimator
Type: Communication channel
Communication latency: 80 ticks
```

Do not show `send_offset`.

For protected Bosch connections:

- source/destination are read-only;
- logical connection latency is read-only zero;
- communication latency follows the project edit policy already established;
- changing latency updates only the transmission delay represented by the public connection field.

## 48. Hover and click behavior

Hovering a connection:

- highlights the arrow and its source/destination ports;
- shows a tooltip with source, destination, kind, and displayed latency;
- may show a one-line transient preview at the top of System Builder;
- does not replace the persistent structural selection.

Clicking a connection:

- updates `StructuralSelection`;
- opens the Connection editor in System Builder;
- keeps the information visible after the mouse leaves.

Clicking a task or resource:

- updates `StructuralSelection`;
- System Builder shows that entity.

This must work:

- before simulation;
- while Paused;
- after Finished.

While Running:

- clicking still changes selection;
- System Builder displays the selected information;
- editable controls are disabled;
- the reason is shown as `Pause the simulation to edit the system.`

## 49. Separate structural and runtime selection

Architecture task/resource/connection clicks drive `StructuralSelection`.

Runtime-specific items continue to drive `RuntimeSelection`.

Do not overload one selection object for both roles.

Have `draw_architecture_view()` return typed interactions rather than mutating unrelated state internally.

Suggested variant:

```cpp
using ArchitectureInteraction =
    std::variant<
        ArchitectureSelectTask,
        ArchitectureSelectResource,
        ArchitectureSelectConnection,
        ArchitectureSelectRuntimeItem,
        ArchitectureMoveTask,
        ArchitectureMoveResource,
        ArchitectureResizeResource,
        ArchitectureReassignTask>;
```

The application layer applies the interaction.

---

# Part H — Compact automatic Architecture layout

## 50. Task box sizing

Task boxes use one visible text line.

Sizing:

```text
horizontal padding: 8 logical pixels per side
vertical padding:   5 logical pixels per side
minimum width:      96 logical pixels
maximum width:      220 logical pixels
height:             one text line + vertical padding
```

Measure the actual display label using ImGui text measurement.

For labels wider than the maximum:

- ellipsize visually;
- show full label in tooltip.

Do not reserve a second text row when no second line is rendered.

## 51. Resource box sizing

A resource container is sized from the bounds of its assigned task boxes.

Automatic size:

```text
resource header height = one text line + 8 logical pixels
inner padding          = 12 logical pixels
task gap               = 12 logical pixels
```

For each resource:

1. determine assigned task bounds;
2. add header area;
3. add inner padding;
4. create the minimum container enclosing all tasks;
5. apply a small minimum size only for an empty resource.

Do not use one global resource height for all resources.

## 52. Remove assignment lines

Do not draw task-to-resource assignment lines.

Containment communicates assignment.

Assignment metadata remains in the graph model for:

- selection;
- task reassignment;
- layout;
- validation.

When a task is selected, lightly highlight its resource boundary instead of drawing an assignment edge.

## 53. Ports

Every task has input and output ports.

Visuals:

```text
input port:  hollow circle
output port: filled circle
```

Preferred sides:

```text
left and top   = input candidates
right and bottom = output candidates
```

For each connection:

- destination mainly right of source:
  - source right output;
  - destination left input;
- destination mainly below source:
  - source bottom output;
  - destination top input;
- destination mainly left:
  - source left-side output only when necessary;
  - destination right-side input only when necessary;
- choose the route minimizing crossings and edge length.

Use orthogonal segments where practical.

Arrowhead terminates at the destination input port.

## 54. Deterministic auto-layout

Retain the existing layered architecture approach where possible, but replace fixed container dimensions.

Auto-layout procedure:

1. build task dependency layers from all logical and communication connections;
2. use stable task ID ordering as the tie-breaker;
3. if a cycle exists, place cycle members in stable ID order in one layer;
4. place tasks according to dependency layer;
5. group tasks visually inside assigned resources;
6. calculate each resource container from its child task bounds;
7. place resource containers left-to-right by the minimum layer of their tasks;
8. use stable resource ID as tie-breaker;
9. run one deterministic crossing-reduction pass using predecessor barycenters;
10. do not iterate indefinitely.

Auto-layout output must be deterministic for the same system.

---

# Part I — Manual Architecture arrangement

## 55. Explicit interaction modes

Add an Architecture toolbar:

```text
Mode: [Select] [Arrange] [Assign]
[Auto Layout] [Reset Layout]
```

### Select mode

- click task/resource/connection to select;
- hover tooltip;
- no movement;
- wheel/drag background may pan if already supported.

### Arrange mode

- drag task to change visual position only;
- drag resource header to move the resource and all contained tasks;
- resize resource through right, bottom, and bottom-right handles;
- moving does not change task assignment;
- task positions remain constrained to the resource interior unless the task is intentionally moved outside and the UI offers `Move to resource` separately.

### Assign mode

- drag a task onto another resource;
- show destination highlight;
- on drop, update the task-resource assignment draft;
- preserve the task’s relative position inside the new resource when possible;
- validate through the existing draft workflow;
- do not apply/restart automatically.

## 56. Manual size behavior

A resource has:

```text
automatic size
optional manual minimum-size override
```

Manual resizing may enlarge a resource beyond automatic size.

It must not shrink below the child-task bounding box plus required padding.

Provide context-menu commands:

```text
Auto Size Resource
Reset Position
Reset Size
```

## 57. Persistence

Store architecture layout in `workspace.json`.

Suggested records:

```cpp
struct GuiNodeLayout {
    double x;
    double y;
};

struct GuiResourceLayout {
    double x;
    double y;
    std::optional<double> width_override;
    std::optional<double> height_override;
};

struct GuiArchitectureLayoutState {
    std::map<TaskId, GuiNodeLayout> task_positions;
    std::map<ResourceId, GuiResourceLayout> resource_layouts;
    GuiArchitectureMode mode;
    double zoom;
    double pan_x;
    double pan_y;
};
```

Use stable IDs in serialized form.

Rules:

- new entities receive auto-layout positions;
- missing records fall back to auto-layout;
- stale records are ignored;
- invalid sizes are clamped;
- old workspaces without layout state load normally;
- `Reset Layout` clears manual overrides and recomputes deterministic auto-layout.

---

# Part J — Results layout refinement

## 58. Buttons at top

When a completed result is Ready, place:

```text
[Open Plot Visualizer...] [Export Completed Results...]
```

at the top of Results.

While finalization is pending:

```text
Finalizing completed run...
```

Buttons remain disabled.

When finalization failed, show the diagnostic.

## 59. Top summary row

For Bosch runs:

```text
+---------------------------+---------------------------+
| Run Summary               | Bosch Summary             |
+---------------------------+---------------------------+
```

For generic runs:

- Run Summary may use the full width; or
- the right side may contain a compact generic Execution Summary.

Use a horizontal splitter between left and right summary cards only when beneficial; otherwise use a stable responsive ratio.

On narrow width, stack the cards vertically.

## 60. Draggable vertical section sizing

The Results vertical structure is:

```text
top summary region
horizontal splitter
Timing and Execution
horizontal splitter
Task Response Times
```

Both splitters are draggable.

Persist:

```cpp
struct ResultsLayoutState {
    float summary_region_ratio;
    float timing_region_ratio_within_remainder;
};
```

Use minimum heights:

```text
summary region:  6 text lines
timing region:   6 text lines
responses:       8 text lines
```

Clamp ratios after DPI or window-size changes.

Do not use hard-coded fixed pixel heights for the main sections.

## 61. Scroll behavior

Each major section uses a child region.

- top summary: local scroll only when required;
- timing: local scroll only when required;
- response table: vertical and horizontal scrolling;
- splitters remain visible.

## 62. Result content

Keep the Goal 5 compact content:

Run Summary:

- canonical events;
- completed jobs;
- deadline misses;
- preemptions;
- horizon.

Timing and Execution:

- sent messages;
- delivered messages;
- paired deliveries;
- undelivered messages;
- min/mean/max communication delay;
- wall-clock duration;
- events per second;
- ticks per second;
- Live/Fast mode;
- batch unit and size.

Bosch Summary:

- threshold crossings/violations;
- maximum absolute lateral error;
- critical-section count or duration;
- relevant completed Bosch indicators.

Task Response Times:

- Task;
- Completed jobs;
- Minimum;
- Mean;
- Maximum;
- Deadline;
- Deadline misses.

Resource utilization remains absent from Results.

---

# Part K — Responsive System Builder

## 63. Problem to solve

In the narrow left sidebar, current full-width input boxes and long inline help text can cover or compete with labels.

The property editor must adapt to available width.

## 64. Responsive breakpoint

Use a logical-width breakpoint based on font size:

```text
wide mode when content width >= 24 × current font size
narrow mode otherwise
```

Do not use raw physical pixels for the breakpoint.

## 65. Wide mode

Use two columns:

```text
Label        Control
```

Rules:

- label column fixed-fit;
- control column does not automatically consume unlimited width;
- maximum ordinary control width approximately `16 × font size`;
- multi-line or complex controls may use remaining width;
- diagnostics appear below the control;
- help text appears below the table across full width.

## 66. Narrow mode

Render each property vertically:

```text
Label
[Control]

Diagnostic/help
```

Do not use a two-column table in narrow mode.

## 67. Protected Bosch help

The Bosch identity explanation must appear below the entire property group:

```text
This task identity is fixed by the Bosch FMU interface.
Timing, execution profile, and allocation remain editable.
```

Do not place this long text inside a narrow value column.

## 68. Read-only state

While Running:

- all structural controls disabled;
- labels and values remain visible;
- selection remains functional;
- show a compact explanation at the top:

```text
Pause the simulation to edit the system.
```

While Paused or Finished:

- editing follows the project edit policy;
- selection from Architecture opens the appropriate entity.

## 69. Scrolling and sizing

System Builder is vertically scrollable.

Input boxes must not overlap:

- labels;
- diagnostics;
- tooltips;
- section headers;
- buttons.

Use scoped IDs so vertical narrow-mode controls remain unique.

---

# Part L — File-level implementation guidance

## 70. Inspect these files first

The coding agent should inspect only the directly relevant files before editing.

Likely files include:

```text
apps/gui/main.cpp
apps/gui/gui_application.cpp
apps/gui/gui_application.hpp

apps/gui/views/architecture_view.cpp
apps/gui/views/architecture_view.hpp
apps/gui/views/event_view.cpp
apps/gui/views/event_view.hpp
apps/gui/views/plot_visualizer.cpp
apps/gui/views/plot_visualizer.hpp
apps/gui/views/results_view.cpp
apps/gui/views/results_view.hpp
apps/gui/views/system_builder.cpp
apps/gui/views/system_builder.hpp

src/cpssim/gui/simulation_controller.cpp
src/cpssim/gui/simulation_controller.hpp
src/cpssim/gui/simulation_session.cpp
src/cpssim/gui/simulation_session.hpp
src/cpssim/gui/architecture_graph.cpp
src/cpssim/gui/architecture_graph.hpp
src/cpssim/gui/workspace_state.cpp
src/cpssim/gui/workspace_state.hpp
src/cpssim/gui/selection.*
src/cpssim/gui/event_table.*
src/cpssim/gui/plot_visualizer_model.*

src/cpssim/analysis/run_result.cpp
src/cpssim/analysis/run_result.hpp
src/cpssim/analysis/completed_run_result.cpp
src/cpssim/analysis/completed_run_result.hpp

src/cpssim/application/bosch_result_analysis.*
src/cpssim/application/project/*
src/cpssim/conformance/bosch_reference.cpp

src/cpssim/model/specifications.hpp
src/cpssim/network/fixed_delay_network.cpp
src/cpssim/bosch/trigger_encoder.cpp

directly related tests and GUI documentation
```

The agent must confirm actual paths before changing code.

## 71. Suggested new files

Use focused components instead of growing `gui_application.cpp` excessively.

Suggested additions:

```text
src/cpssim/gui/gui_frame_scheduler.hpp
src/cpssim/gui/gui_frame_scheduler.cpp

src/cpssim/gui/gui_performance_profiler.hpp
src/cpssim/gui/gui_performance_profiler.cpp

src/cpssim/gui/presentation_publication.hpp
src/cpssim/gui/presentation_publication.cpp

src/cpssim/analysis/completed_run_finalizer.hpp
src/cpssim/analysis/completed_run_finalizer.cpp

src/cpssim/gui/event_view_cache.hpp
src/cpssim/gui/event_view_cache.cpp

src/cpssim/gui/plot_data_cache.hpp
src/cpssim/gui/plot_data_cache.cpp

src/cpssim/gui/architecture_layout.hpp
src/cpssim/gui/architecture_layout.cpp

src/cpssim/gui/architecture_interaction.hpp
```

Names may be adjusted to repository conventions, but responsibilities must remain separated.

---

# Part M — Implementation phases and commits

## 72. Phase 1 — Instrumentation and event-driven frame scheduling

Implement:

- profiler;
- frame activity classifier;
- wait-strategy classifier;
- `glfwPollEvents` for Running/Interactive;
- `glfwWaitEventsTimeout(0.05)` only for BackgroundPending;
- `glfwWaitEvents()` for FullyIdle;
- `glfwPostEmptyEvent()` background wake-up bridge;
- skip the entire Dear ImGui frame when no redraw is required;
- simple pointer-region map for high-value areas;
- suppress passive mouse-move frames;
- one-frame redraw for click, release, wheel, keyboard, and text input;
- BoundarySensitive enter/leave redraw;
- PositionSensitive movement redraw;
- DragHandle interaction frames;
- active-frame cap fallback;
- idle session-update skip.

Commit:

```text
goal6: add GUI profiling and idle-aware frame scheduling
```

## 73. Phase 2 — Presentation publication and immutable completed data

Implement:

- generation counters;
- 15 Hz Live presentation publication;
- no post-Finished snapshot churn;
- shared completed-run dataset;
- exactly-once finalization request.

Commit:

```text
goal6: publish snapshots by generation and share completed data
```

## 74. Phase 3 — Background finalizer

Implement:

- `std::jthread` finalizer;
- cancellation/join;
- Ready/Failed states;
- GUI-thread publication;
- Results finalizing status.

Commit:

```text
goal6: finalize completed results without blocking the GUI
```

## 75. Phase 4 — Canonical Events and Plot caches

Implement:

- event row/projection cache;
- lazy raw JSON;
- debounced text filtering;
- plot data cache;
- one-time Bosch analysis;
- width-based min/max downsampling;
- visible interval/lane rendering.

Commit:

```text
goal6: cache event projections and completed-run plot data
```

## 76. Phase 5 — Connection presentation and selection

Implement:

- unified GUI connection type;
- displayed latency semantics;
- hidden Bosch internal offset;
- hover tooltip;
- click-to-System-Builder selection;
- Running read-only behavior.

Commit:

```text
goal6: unify architecture connections and structural selection
```

## 77. Phase 6 — Architecture layout and manual arrangement

Implement:

- compact task size;
- resource auto-size;
- no assignment lines;
- ports;
- Select/Arrange/Assign modes;
- resource resize;
- layout persistence/reset.

Commit:

```text
goal6: add compact and editable architecture layout
```

## 78. Phase 7 — Results and System Builder layout

Implement:

- top buttons;
- Run/Bosch summary row;
- draggable vertical Result sections;
- responsive property editor;
- protected help placement.

Commit:

```text
goal6: refine results and system builder layouts
```

## 79. Phase 8 — Tests, benchmarks, and documentation

Implement:

- all tests below;
- performance report;
- updated GUI architecture documentation;
- updated timing/connection terminology.

Commit:

```text
goal6: document and verify GUI stabilization
```

---

# Part N — Required automated tests

## 80. Timing invariants

Verify no Goal 6 change affects:

- `[start, finish)` execution intervals;
- completion at `start + execution_time`;
- completion exactly at deadline is on time;
- completion precedes same-tick deadline check;
- output publication occurs at finish tick;
- same-tick downstream read is permitted by phase ordering;
- Bosch trigger conformance remains exact;
- displayed communication latency excludes hidden adapter offset.

## 81. Snapshot publication

Tests:

- no snapshot exists → publish;
- data advances in Live but interval not reached → do not publish;
- interval reached → publish;
- Pause → publish immediately;
- Finished → publish once;
- repeated Finished frames → no additional publish;
- Reset → publish/invalidate as designed;
- Fast to Live → publish;
- generation increments correctly.

## 82. Frame scheduling

Tests for pure activity/wait decisions:

- Running → Poll;
- active ImGui item → Poll;
- splitter drag → Poll;
- Architecture drag/resize → Poll;
- Plot pan/zoom → Poll;
- finalizer running without interaction → WaitWithTimeout;
- Paused, inactive, no background work → WaitIndefinitely;
- Finished, inactive, no background work → WaitIndefinitely;
- completed worker publication requests `glfwPostEmptyEvent`;
- no redraw request in FullyIdle skips `ImGui::NewFrame`;
- a redraw request produces exactly one normal complete frame;
- no unnecessary session update in idle state;
- passive mouse movement requests no redraw;
- click/release requests one redraw;
- wheel requests one redraw;
- entry/exit of a BoundarySensitive region requests one redraw;
- movement inside the same BoundarySensitive region requests no repeated redraw;
- movement inside a PositionSensitive region requests redraw;
- active DragHandle requests interaction frames;
- invalid region map requests one recovery frame;
- DPI/layout invalidation rebuilds the region map safely.

## 83. Completed finalizer

Tests:

- one request per generation;
- successful Ready publication;
- no partial result exposure;
- cancellation on Reset;
- cancellation on project replacement;
- join on destruction;
- worker failure returns diagnostic;
- stale generation result is discarded;
- completed dataset lifetime remains valid.

## 84. Event cache

Tests:

- rows build once per presentation generation;
- source order retained;
- no unnecessary sort;
- filter changes rebuild projection only;
- column changes do not rebuild rows;
- lazy JSON builds only selected row;
- selected event preserved by sequence;
- debounce behavior through testable timer abstraction;
- large synthetic trace filtering correctness.

## 85. Plot cache

Tests:

- Bosch analysis computed once per run;
- unchanged key reuses cache;
- range change invalidates;
- width change invalidates;
- legend/grid/thickness do not invalidate X/Y;
- min/max downsampling preserves first/last and extrema;
- digital transitions preserved;
- full-resolution source remains unchanged;
- nearest sample uses full-resolution data;
- off-screen intervals removed;
- adjacent intervals merged correctly.

## 86. Architecture connection tests

Verify:

- Bosch logical edges display latency 0;
- Bosch communication edges display latency 80;
- hidden send offset is absent from GUI model;
- logical edge creates no message events;
- communication edge retains runtime message semantics;
- hover identifies source/destination/kind/latency;
- click selects connection;
- task click selects task;
- resource click selects resource;
- System Builder shows selection while Paused and Finished;
- controls disabled while Running.

## 87. Architecture layout tests

Verify:

- task height is one-line based;
- resource bounds contain assigned tasks;
- resource sizes differ according to contents;
- assignment lines are absent from draw model;
- ports selected deterministically;
- auto-layout deterministic;
- cycles handled deterministically;
- dragging resource moves children;
- Arrange does not reassign;
- Assign changes draft assignment;
- resource cannot shrink through children;
- workspace round trip;
- stale IDs ignored;
- reset returns auto-layout.

## 88. Results layout tests

Verify:

- buttons at top;
- Bosch summary beside Run Summary at wide width;
- narrow layout stacks;
- splitter ratios clamp;
- ratios persist;
- finalizing/failed/ready states;
- resource utilization absent;
- section content unchanged.

## 89. System Builder tests

Verify:

- wide mode uses two columns;
- narrow mode stacks label/control;
- bounded input widths;
- diagnostics below control;
- Bosch help outside property value cell;
- no overlap through layout model tests where possible;
- read-only selection visible while Running.

## 90. Regression suites

Run and preserve:

- kernel tests;
- scheduler/resource tests;
- network tests;
- canonical event tests;
- Bosch trigger tests;
- Bosch conformance;
- functional model tests;
- GUI controller/session tests;
- project/workspace tests;
- result/export tests;
- CLI tests;
- DPI tests;
- ImPlot lifecycle tests.

---

# Part O — Manual performance verification

## 91. Required reference workflow

Use the pinned Bosch reference project and run to the standard stop tick.

Measure:

- Live mode;
- Fast mode with Events/1000;
- Fast mode with Ticks/1000.

Record:

- wall-clock run duration;
- finalization duration;
- maximum GUI frame stall;
- idle CPU after Finished;
- event cache first-build time;
- event scrolling responsiveness;
- plot first-open time;
- plot pan/zoom responsiveness;
- snapshot publication count;
- event cache rebuild count;
- plot cache rebuild count.

## 92. Post-run idle target

The primary invariant is no repeated expensive work.

Manual target on the user’s Ubuntu/VMware environment:

- CPU usage must fall far below the previous sustained 100%;
- no single CPSSim GUI thread should continuously occupy a full core while idle;
- the interface should respond immediately to tab switches and clicks;
- five seconds of idle Finished state should not change heavy-work counters.

Do not encode a brittle CI CPU-percentage assertion.

## 93. Responsiveness target

Manual targets:

- Pause/Finish button response visible within one active frame;
- Results immediately shows `Finalizing completed run...`;
- no operating-system “not responding” warning during ordinary Bosch completion;
- Canonical Events scrolling remains interactive after cache construction;
- Plot Visualizer pan/zoom does not rebuild data every frame;
- opening/closing the plot does not alter the completed dataset.

---

# Part P — Documentation updates

## 94. Update GUI architecture documentation

Document:

- frame activity and wait-strategy states;
- event-driven Dear ImGui frame scheduling;
- background wake-up through `glfwPostEmptyEvent()`;
- presentation generations;
- completed-data ownership;
- finalizer thread boundary;
- event and plot caches;
- architecture interaction modes;
- connection semantics;
- Results split layout;
- System Builder responsive layout.

## 95. Add timing semantics documentation

Document exactly:

```text
release at beginning of tick r
execution interval [s, f)
finish/output availability at beginning of f
deadline boundary r + D
finish at deadline is valid
same-tick completion precedes reads/deadline check
```

Document Bosch-specific GUI semantics:

```text
displayed communication latency = 80 ticks
hidden adapter handoff is not shown
logical latency = 0 ticks
```

## 96. Performance report

Add a short report containing:

- baseline commit;
- Goal 6 commit;
- test scenario;
- before/after counters;
- before/after observed CPU behavior;
- finalization time;
- event cache time;
- plot cache time;
- known limitations.

---

# Part Q — Acceptance criteria

Goal 6 is complete only when all of the following are true.

## Performance

- No complete snapshot is copied on every Finished frame.
- No completed result is rebuilt on every frame.
- Completed-result finalization does not block the GUI thread.
- Canonical Events rows are cached by generation.
- Canonical Events filtering is cached and debounced.
- Raw event JSON is lazy.
- Bosch plot analysis is built once per run.
- Plot X/Y vectors are cached.
- Idle Paused/Finished GUI uses indefinite event waiting.
- CPSSim does not start a Dear ImGui frame when FullyIdle and no redraw is requested.
- Background completion wakes the GLFW loop through `glfwPostEmptyEvent()`.
- Passive cursor movement does not create a frame.
- Click, release, wheel, keyboard, and text input create one redraw.
- BoundarySensitive regions redraw on enter/leave, not every pixel.
- Only PositionSensitive regions redraw on internal cursor movement.
- Post-run idle CPU is substantially reduced and no periodic 20 Hz redraw occurs when fully idle.
- No ordinary Bosch completion produces a long “not responding” state.

## Architecture

- Logical and communication arrows use one user-facing Connection model.
- Logical arrows display 0-tick communication latency.
- Network arrows display 80-tick Bosch latency.
- Hidden one-tick Bosch adapter timing is not shown.
- Hover provides connection information.
- Click selects tasks, resources, and connections in System Builder.
- Selection works before, during Pause, and after Finish.
- Task boxes use one text row.
- Resource boxes fit their tasks.
- Assignment lines are removed.
- Input/output ports are visible.
- Select/Arrange/Assign modes work.
- Manual layout and size overrides persist.

## Results and System Builder

- Plot and Export buttons are at the top.
- Bosch Summary appears beside Run Summary when width permits.
- Results vertical sections use draggable splitters.
- Results ratios persist.
- System Builder adapts between wide and narrow layouts.
- Input controls do not cover labels/help text.
- Running state remains selectable but read-only.

## Semantics and regressions

- Core timing semantics are unchanged.
- Bosch conformance remains exact.
- Live and Fast outputs remain equal.
- No logical edge creates false network events.
- Project, workspace, export, CLI, and DPI behavior remain valid.

---

# Part R — Known deferrals

Do not implement these in Goal 6:

- Matplot++ publication export;
- multi-run plot comparison;
- parameter sweeps;
- general zero-delay network semantics;
- superdense-time kernel redesign;
- worker-thread simulation;
- arbitrary generic functional-graph editing beyond the existing project model;
- GPU plot decimation;
- remote/headless GUI rendering;
- multi-vehicle Bosch support.
