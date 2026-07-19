/***
 * File: src/cpssim/functional/functional_runtime.cpp
 * Purpose: Enforce generic functional lifecycle, consecutive observation
 *          ticks, typed-signal validity, action timing, and offline replay.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The runtime validates adapter output before changing its own trace.
 ***/

#include "cpssim/functional/functional_runtime.hpp"

#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace cpssim {
namespace {

/***
 * Validates one row's expected tick, finite Real values, nonempty names, and
 * unique names across all three signal types.
 ***/
void validate_observation(const FunctionalObservation& observation, Tick expected_tick) {
    if (observation.tick != expected_tick) {
        throw std::logic_error{"functional model returned an unexpected observation tick"};
    }

    std::set<std::string> names;
    for (const auto& signal : observation.real_signals) {
        if (signal.name.empty() || !std::isfinite(signal.value) ||
            !names.insert(signal.name).second) {
            throw std::logic_error{"functional model returned an invalid Real signal"};
        }
    }
    for (const auto& signal : observation.integer_signals) {
        if (signal.name.empty() || !names.insert(signal.name).second) {
            throw std::logic_error{"functional model returned an invalid Integer signal"};
        }
    }
    for (const auto& signal : observation.boolean_signals) {
        if (signal.name.empty() || !names.insert(signal.name).second) {
            throw std::logic_error{"functional model returned an invalid Boolean signal"};
        }
    }
}

/*** Ensures a replay event sequence is nondecreasing and inside its horizon. ***/
void validate_replay_trace(const std::vector<Event>& trace, Tick stop_tick) {
    Tick previous_tick = 0;
    bool has_previous = false;
    for (const auto& event : trace) {
        if (event.tick() > stop_tick) {
            throw std::invalid_argument{"functional replay event lies beyond the stop tick"};
        }
        if (has_previous && event.tick() < previous_tick) {
            throw std::invalid_argument{"functional replay trace is not ordered by tick"};
        }
        previous_tick = event.tick();
        has_previous = true;
    }
}

} // namespace

/*** Validates run bounds without starting the non-owned model. ***/
FunctionalRuntime::FunctionalRuntime(FunctionalModel& model, PhysicalDuration tick_period,
                                     Tick stop_tick)
    : model_{model}, tick_period_{tick_period}, stop_tick_{stop_tick} {
    if (tick_period_.count() <= 0) {
        throw std::invalid_argument{"functional runtime tick period must be positive"};
    }
    if (stop_tick_ < 0) {
        throw std::invalid_argument{"functional runtime stop tick must not be negative"};
    }
}

/*** Starts the model transactionally and records its required tick-zero row. ***/
std::vector<FunctionalObservation> FunctionalRuntime::initialize() {
    if (initialized_) {
        throw std::logic_error{"functional runtime can initialize only once"};
    }

    auto observation = model_.initialize(tick_period_, stop_tick_);
    validate_observation(observation, 0);
    trace_.push_back(observation);
    initialized_ = true;
    return {std::move(observation)};
}

/***
 * Validates the complete returned batch before appending it, so malformed
 * adapter output cannot partially advance runtime-owned trace state.
 ***/
std::vector<FunctionalObservation> FunctionalRuntime::advance_to(Tick target_tick) {
    if (!initialized_ || finalized_) {
        throw std::logic_error{"functional runtime advance requires an active model"};
    }
    if (target_tick < current_tick_ || target_tick > stop_tick_) {
        throw std::invalid_argument{"functional runtime target tick is outside valid progression"};
    }
    if (target_tick == current_tick_) {
        return {};
    }

    auto observations = model_.advance_to(target_tick);
    const auto expected_size = static_cast<std::size_t>(target_tick - current_tick_);
    if (observations.size() != expected_size) {
        throw std::logic_error{"functional model did not return one row per advanced tick"};
    }
    for (std::size_t index = 0; index < observations.size(); ++index) {
        validate_observation(observations[index], current_tick_ + static_cast<Tick>(index) + 1);
    }

    trace_.insert(trace_.end(), observations.begin(), observations.end());
    current_tick_ = target_tick;
    actions_applied_at_current_tick_ = false;
    return observations;
}

/*** Checks batch identity before delegating any adapter-specific translation. ***/
void FunctionalRuntime::apply_actions(Tick tick, const std::vector<Event>& actions) {
    if (!initialized_ || finalized_) {
        throw std::logic_error{"functional actions require an active model"};
    }
    if (tick != current_tick_) {
        throw std::logic_error{"functional action batch does not use the current tick"};
    }
    if (actions_applied_at_current_tick_) {
        throw std::logic_error{"functional actions were already applied at this tick"};
    }
    for (const auto& event : actions) {
        if (event.tick() != tick) {
            throw std::logic_error{"functional action event differs from its batch tick"};
        }
    }

    model_.apply_actions(tick, actions);
    actions_applied_at_current_tick_ = true;
}

/*** Requires complete in-horizon advancement before finalizing once. ***/
void FunctionalRuntime::finalize() {
    if (!initialized_ || finalized_) {
        throw std::logic_error{"functional runtime finalize requires an active model"};
    }
    if (current_tick_ != stop_tick_) {
        throw std::logic_error{"functional runtime cannot finalize before the stop tick"};
    }
    model_.finalize();
    finalized_ = true;
}

/*** Groups equal-tick events and drives them through the validated runtime. ***/
std::vector<FunctionalObservation>
replay_functional_trace(FunctionalModel& model, PhysicalDuration tick_period, Tick stop_tick,
                        const std::vector<Event>& canonical_trace) {
    validate_replay_trace(canonical_trace, stop_tick);

    FunctionalRuntime runtime{model, tick_period, stop_tick};
    runtime.initialize();
    std::size_t begin = 0;
    while (begin < canonical_trace.size()) {
        const Tick tick = canonical_trace[begin].tick();
        std::size_t end = begin + 1;
        while (end < canonical_trace.size() && canonical_trace[end].tick() == tick) {
            ++end;
        }

        runtime.advance_to(tick);
        const std::vector<Event> actions{
            canonical_trace.begin() + static_cast<std::ptrdiff_t>(begin),
            canonical_trace.begin() + static_cast<std::ptrdiff_t>(end)};
        runtime.apply_actions(tick, actions);
        begin = end;
    }
    runtime.advance_to(stop_tick);
    runtime.finalize();
    return runtime.trace();
}

} // namespace cpssim
