/***
 * File: tests/bosch/trigger_encoder_test.cpp
 * Purpose: Verify all sixteen Bosch mappings, ignored lifecycle events,
 *          Boolean deduplication, strict validation, and exact CSV output.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/bosch/trigger_encoder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cpssim::BoschTrigger;
using cpssim::BoschTriggerEvent;
using cpssim::Event;
using cpssim::EventEntityRefs;
using cpssim::EventPhase;
using cpssim::EventSequence;
using cpssim::EventType;
using cpssim::TaskId;
using cpssim::Tick;

/*** Creates one event with only the task reference needed by this adapter. ***/
Event task_event(Tick tick, std::uint64_t sequence, EventPhase phase, EventType type,
                 std::uint64_t task_id) {
    return Event{tick, phase, EventSequence{sequence}, type,
                 EventEntityRefs{.task_id = TaskId{task_id},
                                 .job_id = std::nullopt,
                                 .resource_id = std::nullopt,
                                 .message_id = std::nullopt,
                                 .vehicle_id = std::nullopt}};
}

/*** Converts encoded records to their external columns for concise checks. ***/
std::vector<std::size_t> columns(const std::vector<BoschTriggerEvent>& events) {
    std::vector<std::size_t> result;
    result.reserve(events.size());
    for (const auto& event : events) {
        result.push_back(cpssim::bosch_trigger_column(event.trigger));
    }
    return result;
}

} // namespace

/*** Verifies every task and network mapping in external column order. ***/
TEST_CASE("Bosch v10 encoding covers all sixteen trigger inputs", "[bosch][trigger]") {
    std::vector<Event> trace;
    std::uint64_t sequence = 0;
    for (std::uint64_t task_id = 1; task_id <= 6; ++task_id) {
        trace.push_back(
            task_event(0, sequence++, EventPhase::Scheduling, EventType::JobStart, task_id));
        trace.push_back(task_event(1, sequence++, EventPhase::ExecutionCompletion,
                                   EventType::JobFinish, task_id));
    }
    trace.push_back(task_event(2, sequence++, EventPhase::CausedAction, EventType::MessageSend, 1));
    trace.push_back(
        task_event(2, sequence++, EventPhase::MessageDelivery, EventType::MessageDelivery, 2));
    trace.push_back(task_event(2, sequence++, EventPhase::CausedAction, EventType::MessageSend, 5));
    trace.push_back(
        task_event(2, sequence++, EventPhase::MessageDelivery, EventType::MessageDelivery, 6));

    const auto encoded = cpssim::encode_bosch_v10_triggers(trace);
    const std::vector<std::size_t> expected_columns{1, 5,  7,  9,  11, 15, 2,  6,
                                                    8, 10, 12, 16, 3,  4,  13, 14};
    const bool mapping_matches = columns(encoded) == expected_columns && encoded.size() == 16;
    REQUIRE(mapping_matches);
    const bool name_matches =
        cpssim::bosch_trigger_name(BoschTrigger::DownlinkReceived) == "downlink_received";
    REQUIRE(name_matches);
}

/*** Verifies that only first dispatch activates and Boolean duplicates collapse. ***/
TEST_CASE("Bosch encoding ignores release resume preemption and deadline events",
          "[bosch][trigger]") {
    const std::vector<Event> trace{
        task_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1),
        task_event(0, 1, EventPhase::Scheduling, EventType::JobStart, 1),
        task_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1),
        task_event(1, 3, EventPhase::Scheduling, EventType::JobPreempt, 1),
        task_event(2, 4, EventPhase::Scheduling, EventType::JobResume, 1),
        task_event(3, 5, EventPhase::DeadlineCheck, EventType::DeadlineMiss, 1),
    };

    const auto encoded = cpssim::encode_bosch_v10_triggers(trace);
    const bool one_activation = encoded.size() == 1 && encoded[0].tick == 0 &&
                                encoded[0].trigger == BoschTrigger::SensorActivated;
    REQUIRE(one_activation);
}

/*** Verifies strict adapter boundaries for task IDs and canonical phases. ***/
TEST_CASE("Bosch encoding rejects malformed mapped events", "[bosch][trigger]") {
    const Event missing_task{0, EventPhase::Scheduling, EventSequence{0}, EventType::JobStart};
    const auto unknown_task = task_event(0, 1, EventPhase::Scheduling, EventType::JobStart, 99);
    const auto wrong_phase = task_event(0, 2, EventPhase::JobRelease, EventType::JobStart, 1);

    REQUIRE_THROWS_AS(cpssim::encode_bosch_v10_triggers({missing_task}), std::logic_error);
    REQUIRE_THROWS_AS(cpssim::encode_bosch_v10_triggers({unknown_task}), std::logic_error);
    REQUIRE_THROWS_AS(cpssim::encode_bosch_v10_triggers({wrong_phase}), std::logic_error);
}

/*** Verifies exact decimal seconds without using floating-point timestamps. ***/
TEST_CASE("Bosch trigger CSV serialization is deterministic", "[bosch][trigger][csv]") {
    const std::vector<BoschTriggerEvent> events{
        {.tick = 0, .trigger = BoschTrigger::SensorActivated},
        {.tick = 3, .trigger = BoschTrigger::ControllerFinished},
        {.tick = 50, .trigger = BoschTrigger::SensorActivated},
        {.tick = 150000, .trigger = BoschTrigger::ActuatorActivated},
    };
    const std::string expected = "eventTick,eventTimeSec,triggerColumn,triggerName\n"
                                 "0,0,1,sensor_activated\n"
                                 "3,0.0003,8,controller_finished\n"
                                 "50,0.005,1,sensor_activated\n"
                                 "150000,15,15,actuator_activated\n";

    const bool csv_matches = cpssim::serialize_bosch_v10_trigger_csv(events) == expected;
    REQUIRE(csv_matches);
    REQUIRE_THROWS_AS(cpssim::serialize_bosch_v10_trigger_csv(
                          {{.tick = -1, .trigger = BoschTrigger::SensorActivated}}),
                      std::invalid_argument);
}
