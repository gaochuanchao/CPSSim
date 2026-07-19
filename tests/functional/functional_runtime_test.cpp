/***
 * File: tests/functional/functional_runtime_test.cpp
 * Purpose: Verify generic functional lifecycle, integer-tick advancement,
 *          action timing, deterministic mock behavior, and offline replay.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: These tests contain no FMI or Bosch types.
 ***/

#include "cpssim/functional/functional_runtime.hpp"
#include "cpssim/functional/mock_functional_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

/*** Creates one simple accepted action with a stable sequence. ***/
cpssim::Event action(cpssim::Tick tick, std::uint64_t sequence) {
    return {tick, cpssim::EventPhase::Scheduling, cpssim::EventSequence{sequence},
            cpssim::EventType::JobStart};
}

/*** Finds one named Real signal or fails the test helper contract. ***/
double real_signal(const cpssim::FunctionalObservation& observation, std::string_view name) {
    for (const auto& signal : observation.real_signals) {
        if (signal.name == name) {
            return signal.value;
        }
    }
    throw std::logic_error{"functional test observation lacks the requested Real signal"};
}

/*** Compares every typed signal field without speculative operators. ***/
bool same_observation(const cpssim::FunctionalObservation& left,
                      const cpssim::FunctionalObservation& right) {
    if (left.tick != right.tick || left.real_signals.size() != right.real_signals.size() ||
        left.integer_signals.size() != right.integer_signals.size() ||
        left.boolean_signals.size() != right.boolean_signals.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.real_signals.size(); ++index) {
        if (left.real_signals[index].name != right.real_signals[index].name ||
            left.real_signals[index].value != right.real_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.integer_signals.size(); ++index) {
        if (left.integer_signals[index].name != right.integer_signals[index].name ||
            left.integer_signals[index].value != right.integer_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.boolean_signals.size(); ++index) {
        if (left.boolean_signals[index].name != right.boolean_signals[index].name ||
            left.boolean_signals[index].value != right.boolean_signals[index].value) {
            return false;
        }
    }
    return true;
}

/*** Compares complete functional traces in deterministic row order. ***/
bool same_trace(const std::vector<cpssim::FunctionalObservation>& left,
                const std::vector<cpssim::FunctionalObservation>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!same_observation(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

/*** Verifies that actions at t first affect the mock observation at t + 1. ***/
TEST_CASE("functional runtime observes before same-tick actions", "[functional][runtime]") {
    cpssim::MockFunctionalModel model{2.0};
    cpssim::FunctionalRuntime runtime{model, std::chrono::microseconds{100}, 3};

    const auto initial = runtime.initialize();
    runtime.apply_actions(0, {action(0, 0)});
    const auto first_advance = runtime.advance_to(2);
    runtime.apply_actions(2, {action(2, 1)});
    const auto second_advance = runtime.advance_to(3);
    runtime.finalize();

    const bool values_match =
        initial.size() == 1 && initial[0].tick == 0 &&
        real_signal(initial[0], "mock_state") == 2.0 && first_advance.size() == 2 &&
        first_advance[0].tick == 1 && real_signal(first_advance[0], "mock_state") == 3.0 &&
        first_advance[1].tick == 2 && real_signal(first_advance[1], "mock_state") == 3.0 &&
        second_advance.size() == 1 && second_advance[0].tick == 3 &&
        real_signal(second_advance[0], "mock_state") == 4.0;
    REQUIRE(values_match);
    REQUIRE(model.finalized());
}

/*** Verifies lifecycle, horizon, event-tick, and one-batch invariants. ***/
TEST_CASE("functional runtime rejects invalid progression", "[functional][runtime][error]") {
    cpssim::MockFunctionalModel model;
    REQUIRE_THROWS_AS((cpssim::FunctionalRuntime{model, std::chrono::microseconds{100}, -1}),
                      std::invalid_argument);

    cpssim::FunctionalRuntime runtime{model, std::chrono::microseconds{100}, 2};
    REQUIRE_THROWS_AS(runtime.advance_to(1), std::logic_error);
    runtime.initialize();
    REQUIRE_THROWS_AS(runtime.initialize(), std::logic_error);
    REQUIRE_THROWS_AS(runtime.apply_actions(0, {action(1, 0)}), std::logic_error);
    runtime.apply_actions(0, {action(0, 0)});
    REQUIRE_THROWS_AS(runtime.apply_actions(0, {}), std::logic_error);
    REQUIRE_THROWS_AS(runtime.advance_to(3), std::invalid_argument);
    runtime.advance_to(1);
    REQUIRE_THROWS_AS(runtime.advance_to(0), std::invalid_argument);
    REQUIRE_THROWS_AS(runtime.finalize(), std::logic_error);
    runtime.advance_to(2);
    runtime.finalize();
    REQUIRE_THROWS_AS(runtime.finalize(), std::logic_error);
}

/*** Verifies completed canonical events reproduce the online functional trace. ***/
TEST_CASE("functional offline replay equals the online mock trace", "[functional][replay]") {
    const std::vector<cpssim::Event> events{action(0, 0), action(2, 1)};

    cpssim::MockFunctionalModel online_model{2.0};
    cpssim::FunctionalRuntime online{online_model, std::chrono::microseconds{100}, 3};
    online.initialize();
    online.apply_actions(0, {events[0]});
    online.advance_to(2);
    online.apply_actions(2, {events[1]});
    online.advance_to(3);
    online.finalize();

    cpssim::MockFunctionalModel replay_model{2.0};
    const auto replay =
        cpssim::replay_functional_trace(replay_model, std::chrono::microseconds{100}, 3, events);

    REQUIRE(same_trace(online.trace(), replay));
}

/*** Verifies replay rejects unordered and out-of-horizon canonical events. ***/
TEST_CASE("functional replay validates its canonical input", "[functional][replay][error]") {
    cpssim::MockFunctionalModel unordered_model;
    const std::vector<cpssim::Event> unordered{action(2, 0), action(1, 1)};
    REQUIRE_THROWS_AS(cpssim::replay_functional_trace(unordered_model,
                                                      std::chrono::microseconds{100}, 3, unordered),
                      std::invalid_argument);

    cpssim::MockFunctionalModel outside_model;
    const std::vector<cpssim::Event> outside{action(4, 0)};
    REQUIRE_THROWS_AS(
        cpssim::replay_functional_trace(outside_model, std::chrono::microseconds{100}, 3, outside),
        std::invalid_argument);
}
