/***
 * File: tests/kernel/event_queue_test.cpp
 * Purpose: Verify sequence allocation, explicit phase precedence, stable
 *          same-time ordering, and queue failure behavior.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/kernel/event_queue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <stdexcept>

namespace {

using cpssim::EventPhase;
using cpssim::EventQueue;
using cpssim::EventSequence;
using cpssim::EventType;
using cpssim::JobId;

/*** Verifies zero-based sequence allocation and stable insertion tie-breaking. ***/
TEST_CASE("the event queue assigns stable insertion sequences", "[kernel][event_queue]") {
    EventQueue queue;

    const auto first = queue.schedule(10, EventPhase::JobRelease, EventType::JobRelease);
    const auto second = queue.schedule(10, EventPhase::JobRelease, EventType::JobRelease);
    const auto third = queue.schedule(10, EventPhase::JobRelease, EventType::JobRelease);

    const auto assignments_match =
        first == EventSequence{0} && second == EventSequence{1} && third == EventSequence{2};
    const auto size_matches = queue.size() == 3;
    REQUIRE(assignments_match);
    REQUIRE(size_matches);

    const auto first_event = queue.pop_next();
    const auto second_event = queue.pop_next();
    const auto third_event = queue.pop_next();
    const auto order_matches = first_event.sequence() == EventSequence{0} &&
                               second_event.sequence() == EventSequence{1} &&
                               third_event.sequence() == EventSequence{2};

    REQUIRE(order_matches);
    REQUIRE(queue.empty());
}

/*** Verifies that an earlier tick takes precedence over every phase value. ***/
TEST_CASE("event tick is the primary queue key", "[kernel][event_queue]") {
    EventQueue queue;
    queue.schedule(20, EventPhase::ExecutionCompletion, EventType::JobFinish);
    queue.schedule(10, EventPhase::CausedAction, EventType::JobStart);

    const auto first = queue.pop_next();
    const auto second = queue.pop_next();
    const auto ticks_match = first.tick() == 10 && second.tick() == 20;

    REQUIRE(ticks_match);
}

/*** Verifies all accepted same-tick phases independently of enum declaration order. ***/
TEST_CASE("same-tick events follow explicit phase precedence", "[kernel][event_queue]") {
    EventQueue queue;
    const std::array insertion_order{
        EventPhase::CausedAction,        EventPhase::Scheduling,    EventPhase::PolicyUpdate,
        EventPhase::JobRelease,          EventPhase::DeadlineCheck, EventPhase::MessageDelivery,
        EventPhase::ExecutionCompletion,
    };
    const std::array expected_order{
        EventPhase::ExecutionCompletion, EventPhase::MessageDelivery, EventPhase::DeadlineCheck,
        EventPhase::JobRelease,          EventPhase::PolicyUpdate,    EventPhase::Scheduling,
        EventPhase::CausedAction,
    };

    for (const auto phase : insertion_order) {
        queue.schedule(10, phase, EventType::JobRelease);
    }

    for (const auto expected_phase : expected_order) {
        const auto phase_matches = queue.pop_next().phase() == expected_phase;
        REQUIRE(phase_matches);
    }
}

/*** Verifies that assigned sequence values support a backward causal reference. ***/
TEST_CASE("scheduled events retain causal sequence references", "[kernel][event_queue]") {
    EventQueue queue;
    const auto cause = queue.schedule(10, EventPhase::JobRelease, EventType::JobRelease);
    queue.schedule(10, EventPhase::Scheduling, EventType::JobStart,
                   {.task_id = std::nullopt,
                    .job_id = JobId{4},
                    .resource_id = std::nullopt,
                    .message_id = std::nullopt,
                    .vehicle_id = std::nullopt},
                   cause);

    queue.pop_next();
    const auto effect = queue.pop_next();
    const auto cause_matches = effect.cause_sequence() == std::optional<EventSequence>{cause};

    REQUIRE(cause_matches);
}

/***
 * Verifies empty access failures and that Event validation fails before the
 * queue consumes its next sequence value.
 ***/
TEST_CASE("event queue failures preserve queue state", "[kernel][event_queue]") {
    EventQueue queue;

    REQUIRE_THROWS_AS(queue.next(), std::out_of_range);
    REQUIRE_THROWS_AS(queue.pop_next(), std::out_of_range);
    REQUIRE_THROWS_AS(queue.schedule(-1, EventPhase::JobRelease, EventType::JobRelease),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        queue.schedule(0, EventPhase::Scheduling, EventType::JobStart, {}, EventSequence{0}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(queue.schedule(0, static_cast<EventPhase>(100), EventType::JobRelease),
                      std::logic_error);

    const auto assigned = queue.schedule(0, EventPhase::JobRelease, EventType::JobRelease);
    const auto state_matches = assigned == EventSequence{0} && queue.size() == 1 &&
                               queue.next().sequence() == EventSequence{0};

    REQUIRE(state_matches);
}

} // namespace
