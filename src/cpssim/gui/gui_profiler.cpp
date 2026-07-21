/*** Implement GUI profiling counters without a graphics dependency. ***/

#include "cpssim/gui/gui_profiler.hpp"

#include <algorithm>

namespace cpssim {

void GuiProfiler::increment(GuiProfileCounter counter, std::uint64_t amount) noexcept {
    values_.counters[static_cast<std::size_t>(counter)] += amount;
}

void GuiProfiler::record(GuiProfileTimer timer, std::chrono::nanoseconds duration) noexcept {
    const auto index = static_cast<std::size_t>(timer);
    const auto milliseconds = std::chrono::duration<double, std::milli>(duration).count();
    values_.last_milliseconds[index] = milliseconds;
    values_.maximum_milliseconds[index] =
        std::max(values_.maximum_milliseconds[index], milliseconds);
}

GuiScopedProfileTimer::GuiScopedProfileTimer(GuiProfiler& profiler, GuiProfileTimer timer) noexcept
    : profiler_{profiler}, timer_{timer}, started_{std::chrono::steady_clock::now()} {}

GuiScopedProfileTimer::~GuiScopedProfileTimer() {
    profiler_.record(timer_, std::chrono::steady_clock::now() - started_);
}

} // namespace cpssim
