/***
 * File: src/cpssim/model/time.cpp
 * Purpose: Implement checked, exact conversion between canonical ticks and
 *          integer nanosecond durations.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Conversion never rounds. Rejecting invalid and inexact input protects
 *        deterministic event timing across platforms.
 ***/

#include "cpssim/model/time.hpp"

#include <limits>
#include <stdexcept>

namespace cpssim {
namespace {

/***
 * Validates the shared requirement that one tick has positive physical
 * duration. Throws std::invalid_argument when the period is zero or negative.
 ***/
void validate_tick_period(PhysicalDuration tick_period) {
    if (tick_period.count() <= 0) {
        throw std::invalid_argument{"tick period must be positive"};
    }
}

} // namespace

/***
 * Validates the period and divides a physical duration only when it is a
 * nonnegative exact multiple, preserving the no-rounding time contract.
 ***/
Tick duration_to_ticks(PhysicalDuration duration, PhysicalDuration tick_period) {
    validate_tick_period(tick_period);

    if (duration.count() < 0) {
        throw std::invalid_argument{"physical duration must not be negative"};
    }

    if (duration.count() % tick_period.count() != 0) {
        throw std::invalid_argument{"physical duration is not an exact number of ticks"};
    }

    return duration.count() / tick_period.count();
}

/***
 * Validates the inputs and multiplies ticks by the period after checking that
 * the result fits in the nanosecond representation.
 ***/
PhysicalDuration ticks_to_duration(Tick ticks, PhysicalDuration tick_period) {
    validate_tick_period(tick_period);

    if (ticks < 0) {
        throw std::invalid_argument{"tick count must not be negative"};
    }

    const auto period_count = tick_period.count();
    const auto maximum_count = std::numeric_limits<PhysicalDuration::rep>::max();

    if (ticks > maximum_count / period_count) {
        throw std::overflow_error{"physical duration exceeds its representation"};
    }

    return PhysicalDuration{ticks * period_count};
}

} // namespace cpssim
