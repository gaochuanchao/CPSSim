/***
 * File: tests/trace/event_json_test.cpp
 * Purpose: Verify the exact deterministic JSON Lines representation of
 *          canonical events with present and absent optional references.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

namespace {

using cpssim::Event;
using cpssim::EventEntityRefs;
using cpssim::EventPhase;
using cpssim::EventSequence;
using cpssim::EventType;
using cpssim::JobId;
using cpssim::MessageId;
using cpssim::ResourceId;
using cpssim::serialize_event_json_line;
using cpssim::TaskId;
using cpssim::VehicleId;

/***
 * Verifies exact field order, enum spellings, typed IDs, causal sequence, and
 * the single terminating newline for a fully identified scheduling event.
 ***/
TEST_CASE("event JSON Lines serialization is byte stable", "[trace][json]") {
    const Event event{
        25,
        EventPhase::Scheduling,
        EventSequence{7},
        EventType::JobStart,
        EventEntityRefs{.task_id = TaskId{2},
                        .job_id = JobId{11},
                        .resource_id = ResourceId{3},
                        .message_id = std::nullopt,
                        .vehicle_id = VehicleId{4}},
        EventSequence{5},
    };

    const std::string expected =
        R"json({"schema_version":1,"tick":25,"phase":"scheduling","sequence":7,"type":"job_start","entities":{"task_id":2,"job_id":11,"resource_id":3,"message_id":null,"vehicle_id":4},"cause_sequence":5})json"
        "\n";
    const auto first_line = serialize_event_json_line(event);
    const auto second_line = serialize_event_json_line(event);
    const auto expected_bytes_match = first_line == expected;
    const auto repeated_bytes_match = second_line == first_line;

    REQUIRE(expected_bytes_match);
    REQUIRE(repeated_bytes_match);
}

/*** Verifies stable trace vocabulary for causal network observations. ***/
TEST_CASE("event JSON Lines serializes message event types", "[trace][json][network]") {
    const Event send{
        6,
        EventPhase::CausedAction,
        EventSequence{8},
        EventType::MessageSend,
        EventEntityRefs{.task_id = TaskId{1},
                        .job_id = JobId{3},
                        .resource_id = std::nullopt,
                        .message_id = MessageId{2},
                        .vehicle_id = std::nullopt},
        EventSequence{7},
    };
    const Event delivery{
        86,
        EventPhase::MessageDelivery,
        EventSequence{9},
        EventType::MessageDelivery,
        EventEntityRefs{.task_id = TaskId{4},
                        .job_id = std::nullopt,
                        .resource_id = std::nullopt,
                        .message_id = MessageId{2},
                        .vehicle_id = std::nullopt},
        EventSequence{8},
    };

    const std::string expected_send =
        R"json({"schema_version":1,"tick":6,"phase":"caused_action","sequence":8,"type":"message_send","entities":{"task_id":1,"job_id":3,"resource_id":null,"message_id":2,"vehicle_id":null},"cause_sequence":7})json"
        "\n";
    const std::string expected_delivery =
        R"json({"schema_version":1,"tick":86,"phase":"message_delivery","sequence":9,"type":"message_delivery","entities":{"task_id":4,"job_id":null,"resource_id":null,"message_id":2,"vehicle_id":null},"cause_sequence":8})json"
        "\n";

    const auto send_bytes_match = serialize_event_json_line(send) == expected_send;
    const auto delivery_bytes_match = serialize_event_json_line(delivery) == expected_delivery;
    REQUIRE(send_bytes_match);
    REQUIRE(delivery_bytes_match);
}

/***
 * Verifies that every absent optional reference and cause is represented by an
 * explicit JSON null rather than by an unstable omitted-field convention.
 ***/
TEST_CASE("event JSON Lines serialization writes absent references as null", "[trace][json]") {
    const Event event{0, EventPhase::JobRelease, EventSequence{0}, EventType::JobRelease};
    const std::string expected =
        R"json({"schema_version":1,"tick":0,"phase":"job_release","sequence":0,"type":"job_release","entities":{"task_id":null,"job_id":null,"resource_id":null,"message_id":null,"vehicle_id":null},"cause_sequence":null})json"
        "\n";
    const auto null_bytes_match = serialize_event_json_line(event) == expected;

    REQUIRE(null_bytes_match);
}

} // namespace
