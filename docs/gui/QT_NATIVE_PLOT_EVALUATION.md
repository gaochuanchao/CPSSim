# Qt Native Plot Prototype Evaluation

Goal 7 deliberately does not select a permanent native plotting dependency.
The first Qt frontend prototype uses `QPainter` only as a thin view adapter over
the existing graphics-independent `GuiPlotDataCache`, `GuiPlotLane`, and
full-resolution `GuiSignalModel`.

## Current prototype

- input is the immutable `CompletedRunResult` only;
- lanes are grouped by unit and scalar kind;
- ticks remain exact integers in source data;
- the existing min/max downsampler bounds drawing data to
  `clamp(2 × width, 512, 8192)` points;
- cursor clicks update the shared tick selection;
- Bosch threshold, critical-section, deadline-miss, and selected-tick overlays
  reuse the completed Bosch analysis; and
- export continues to read full-resolution data, never painted points.

An offscreen Debug benchmark on the Ubuntu 24.04 development container projected
100,000 real samples at a 1,920-pixel logical width to 3,840 drawing points in
7.979 ms. The test verifies that all 100,000 source samples remain unchanged.
This is a cache/projection measurement, not a claim about desktop paint or GPU
performance.

## Dependency decision

No third-party native plotting library is adopted in Goal 7. Therefore there is
no new plotting license, pin, or Qt compatibility claim. A later permanent
selection requires a repeatable interactive benchmark, Qt 6.4 compatibility
check, license review, and a dedicated ADR. The current prototype intentionally
does not attempt to become a general-purpose plotting framework.

Desktop zoom/pan feel, dense multi-lane rendering, and multi-DPI painting remain
manual parity checks. The immutable analysis and cache boundary is the stable
part intended for reuse by any future renderer.
