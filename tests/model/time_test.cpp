/***
 * File: tests/model/time_test.cpp
 * Purpose: Verify exact integer conversion between physical nanoseconds and
 *          canonical simulation ticks, including all rejection boundaries.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/time.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using cpssim::duration_to_ticks;
using cpssim::PhysicalDuration;
using cpssim::Tick;
using cpssim::ticks_to_duration;

using namespace std::chrono_literals;

static_assert(std::is_same_v<Tick, std::int64_t>);

/*** Verifies exact round-trip conversion using the Bosch 100-microsecond tick. ***/
TEST_CASE("the Bosch time quantum converts exactly", "[model][time]") {
    const auto duration = std::chrono::duration_cast<PhysicalDuration>(15s);
    const auto tick_period = std::chrono::duration_cast<PhysicalDuration>(100us);

    const auto ticks = duration_to_ticks(duration, tick_period);
    const auto reconstructed_duration = ticks_to_duration(ticks, tick_period);
    const auto tick_count_matches = ticks == 150'000;
    const auto duration_matches = reconstructed_duration == duration;

    REQUIRE(tick_count_matches);
    REQUIRE(duration_matches);
}

/*** Verifies rejection of negative, inexact, and nonpositive duration inputs. ***/
TEST_CASE("duration conversion rejects invalid values", "[model][time]") {
    const auto tick_period = std::chrono::duration_cast<PhysicalDuration>(100us);

    REQUIRE_THROWS_AS(duration_to_ticks(-1ns, tick_period), std::invalid_argument);
    REQUIRE_THROWS_AS(duration_to_ticks(1ns, tick_period), std::invalid_argument);
    REQUIRE_THROWS_AS(duration_to_ticks(1ms, 0ns), std::invalid_argument);
    REQUIRE_THROWS_AS(duration_to_ticks(1ms, -1ns), std::invalid_argument);
}

/*** Verifies rejection of invalid tick inputs and multiplication overflow. ***/
TEST_CASE("tick conversion rejects invalid values and overflow", "[model][time]") {
    REQUIRE_THROWS_AS(ticks_to_duration(-1, 1ns), std::invalid_argument);
    REQUIRE_THROWS_AS(ticks_to_duration(1, 0ns), std::invalid_argument);
    REQUIRE_THROWS_AS(ticks_to_duration(1, -1ns), std::invalid_argument);
    REQUIRE_THROWS_AS(ticks_to_duration(std::numeric_limits<Tick>::max(), 2ns),
                      std::overflow_error);
}

} // namespace
