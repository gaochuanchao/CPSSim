/*** Low-overhead development counters for GUI scheduling and caches. ***/

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cpssim {

enum class GuiProfileCounter : std::size_t {
    Poll,
    TimedWait,
    IndefiniteWait,
    RenderedFrame,
    SkippedFrame,
    BackgroundWakeup,
    SnapshotBuild,
    ResultBuild,
    EventCacheBuild,
    EventFilterBuild,
    PlotCacheBuild,
    BoschAnalysisBuild,
    Count,
};

enum class GuiProfileTimer : std::size_t {
    ControllerUpdate,
    SnapshotBuild,
    ResultFinalization,
    EventCacheBuild,
    PlotCacheBuild,
    ViewModelBuild,
    ImGuiBuild,
    RenderSwap,
    Frame,
    Count,
};

struct GuiProfilerSnapshot {
    std::array<std::uint64_t, static_cast<std::size_t>(GuiProfileCounter::Count)> counters{};
    std::array<double, static_cast<std::size_t>(GuiProfileTimer::Count)> last_milliseconds{};
    std::array<double, static_cast<std::size_t>(GuiProfileTimer::Count)> maximum_milliseconds{};
};

class GuiProfiler {
  public:
    void increment(GuiProfileCounter counter, std::uint64_t amount = 1) noexcept;
    void record(GuiProfileTimer timer, std::chrono::nanoseconds duration) noexcept;
    GuiProfilerSnapshot snapshot() const noexcept { return values_; }
    void reset() noexcept { values_ = {}; }

  private:
    GuiProfilerSnapshot values_;
};

class GuiScopedProfileTimer {
  public:
    GuiScopedProfileTimer(GuiProfiler& profiler, GuiProfileTimer timer) noexcept;
    ~GuiScopedProfileTimer();

  private:
    GuiProfiler& profiler_;
    GuiProfileTimer timer_;
    std::chrono::steady_clock::time_point started_;
};

} // namespace cpssim
