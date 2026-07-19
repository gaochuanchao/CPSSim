/***
 * File: src/cpssim/model/time.hpp
 * Purpose: Define canonical integer simulation time and exact conversions at
 *          the physical-time boundary.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Tick is the canonical representation. PhysicalDuration is used only
 *        when an explicit boundary must express nanoseconds, as required by
 *        ADR-0001.
 ***/

#pragma once

#include <chrono>
#include <cstdint>

namespace cpssim {

/*** Canonical logical time used by the simulator and its event traces. ***/
using Tick = std::int64_t;

/*** Integer physical duration used only at explicit conversion boundaries. ***/
using PhysicalDuration = std::chrono::nanoseconds;

/***
 * Converts a nonnegative physical duration to an exact number of ticks.
 * Parameters: duration is the physical interval; tick_period is the positive
 * duration represented by one simulator tick.
 * Returns: the exact integer tick count.
 * Throws: std::invalid_argument for a nonpositive period, negative duration,
 * or duration that is not an exact multiple of the period.
 ***/
Tick duration_to_ticks(PhysicalDuration duration, PhysicalDuration tick_period);

/***
 * Converts a nonnegative tick count to a physical duration.
 * Parameters: ticks is the canonical count; tick_period is the positive
 * physical duration represented by one tick.
 * Returns: the exactly represented nanosecond duration.
 * Throws: std::invalid_argument for negative ticks or a nonpositive period,
 * and std::overflow_error when multiplication exceeds PhysicalDuration.
 ***/
PhysicalDuration ticks_to_duration(Tick ticks, PhysicalDuration tick_period);

} // namespace cpssim
