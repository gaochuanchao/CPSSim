# GUI Performance Verification

This record defines the repeatable Goal 6 GUI performance check. Counters and
timings come from **View → GUI Profiler** in a Debug/development build; they are
observational and never influence simulation.

## Reference workflow

Open the pinned Bosch reference project and run to its standard stop tick in:

1. Live;
2. Fast, Events/1000;
3. Fast, Ticks/1000.

For each run record wall-clock duration, result-finalization duration, maximum
complete-frame stall, snapshot count, first event-cache build time/count, first
plot-cache build time/count, and Bosch-analysis/result-build counts. Compare
final traces, observations, and resource snapshots; all three modes must be
identical.

After a run is Ready, leave it fully idle for five seconds. The rendered-frame,
snapshot/result/event/plot/Bosch-analysis counters must remain unchanged;
Indefinite wait must be used without periodic Timed wait. Passive Results or
background cursor motion must render no frame. A click, wheel action, and a
BoundarySensitive enter/leave each request one frame; movement within the same
boundary does not. Plot/Timeline motion requests coordinate feedback without
rebuilding plot X/Y data.

## 2026-07-21 automated evidence

- Pure scheduler tests cover all four activity states, all three wait
  strategies, redraw generations, stale-map recovery, passive motion,
  boundary enter/inside/leave, and position-sensitive motion.
- Generation tests cover the 15 Hz Live gate and immediate coherent
  publication boundaries.
- Finalizer tests cover immutable request publication, cancellation/join,
  failure, generation replacement, and single Ready adoption.
- Event and plot tests cover generation keys, canonical ordering, debounced
  filters, deterministic point budgets, extrema/digital preservation, visible
  interval merging, and cache reuse.
- Live/Fast equivalence, exact Bosch results, canonical order, GUI DPI, and
  sanitizer/release profiles are part of the repository verification suite.

The automated container has no X11 display and no Xvfb executable. Consequently
desktop-only wall-clock values, GUI frame stalls, idle CPU sampling, event
scroll feel, plot pan/zoom feel, and five-second native-loop counter readings
were not fabricated here. Run the reference workflow above on the target
Ubuntu/VMware desktop and append the measured table without changing semantic
expectations.

| Mode | Run duration | Finalization | Max frame stall | Idle CPU |
|---|---:|---:|---:|---:|
| Live | desktop check required | desktop check required | desktop check required | desktop check required |
| Fast Events/1000 | desktop check required | desktop check required | desktop check required | desktop check required |
| Fast Ticks/1000 | desktop check required | desktop check required | desktop check required | desktop check required |
