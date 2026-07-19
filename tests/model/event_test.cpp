/***
 * File: tests/model/event_test.cpp
 * Purpose: Verify canonical event construction, immutable field access, typed
 *          entity references, and local validation failures.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/event.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

namespace {

using cpssim::Event;
using cpssim::EventEntityRefs;
using cpssim::EventPhase;
using cpssim::EventSequence;
using cpssim::EventType;
using cpssim::JobId;
using cpssim::ResourceId;
using cpssim::TaskId;

/***
 * Verifies that a valid event preserves its canonical fields and typed entity
 * references without adding mutable behavior.
 ***/
TEST_CASE("canonical events preserve validated fields", "[model][event]") {
    const Event event{
        25,
        EventPhase::Scheduling,
        EventSequence{7},
        EventType::JobStart,
        EventEntityRefs{.task_id = TaskId{2},
                        .job_id = JobId{11},
                        .resource_id = ResourceId{3},
                        .message_id = std::nullopt,
                        .vehicle_id = std::nullopt},
        EventSequence{5},
    };

    const auto fields_match =
        event.tick() == 25 && event.phase() == EventPhase::Scheduling &&
        event.sequence() == EventSequence{7} && event.type() == EventType::JobStart &&
        event.entities().task_id == std::optional<TaskId>{TaskId{2}} &&
        event.entities().job_id == std::optional<JobId>{JobId{11}} &&
        event.entities().resource_id == std::optional<ResourceId>{ResourceId{3}} &&
        !event.entities().message_id.has_value() && !event.entities().vehicle_id.has_value() &&
        event.cause_sequence() == std::optional<EventSequence>{EventSequence{5}};

    REQUIRE(fields_match);
}

/***
 * Verifies that events reject negative logical time and causal references that
 * do not point to an earlier append-order sequence.
 ***/
TEST_CASE("canonical events reject invalid time and causality", "[model][event]") {
    REQUIRE_THROWS_AS((Event{-1, EventPhase::JobRelease, EventSequence{0}, EventType::JobRelease}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((Event{0,
                             EventPhase::Scheduling,
                             EventSequence{5},
                             EventType::JobStart,
                             {},
                             EventSequence{5}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((Event{0,
                             EventPhase::Scheduling,
                             EventSequence{5},
                             EventType::JobStart,
                             {},
                             EventSequence{6}}),
                      std::invalid_argument);
}

} // namespace
