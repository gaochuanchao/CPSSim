/*** Verify deterministic GUI profiler counters and duration summaries. ***/

#include "cpssim/gui/gui_profiler.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace cpssim;

TEST_CASE("GUI profiler accumulates counters and retains last and maximum timings",
          "[gui][profiler]") {
    GuiProfiler profiler;
    profiler.increment(GuiProfileCounter::RenderedFrame);
    profiler.increment(GuiProfileCounter::RenderedFrame, 2);
    profiler.record(GuiProfileTimer::Frame, std::chrono::milliseconds{4});
    profiler.record(GuiProfileTimer::Frame, std::chrono::milliseconds{2});

    const auto values = profiler.snapshot();
    REQUIRE(values.counters[static_cast<std::size_t>(GuiProfileCounter::RenderedFrame)] == 3);
    REQUIRE(values.last_milliseconds[static_cast<std::size_t>(GuiProfileTimer::Frame)] == 2.0);
    REQUIRE(values.maximum_milliseconds[static_cast<std::size_t>(GuiProfileTimer::Frame)] == 4.0);

    profiler.reset();
    REQUIRE(profiler.snapshot().counters == GuiProfilerSnapshot{}.counters);
}
