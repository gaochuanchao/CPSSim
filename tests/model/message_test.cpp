/***
 * File: tests/model/message_test.cpp
 * Purpose: Verify generic message identity, endpoint, timing, causality, and
 *          initial lifecycle invariants independently of network behavior.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/message.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

namespace {

using cpssim::EventSequence;
using cpssim::JobId;
using cpssim::JobIdentity;
using cpssim::Message;
using cpssim::MessageId;
using cpssim::MessageLifecycle;
using cpssim::MessageTiming;
using cpssim::TaskId;

/*** Verifies immutable endpoints, planned timing, cause, and PendingSend state. ***/
TEST_CASE("a new message captures its causal source and planned delivery", "[model][message]") {
    const Message message{MessageId{4}, JobIdentity{TaskId{1}, JobId{7}}, TaskId{2},
                          MessageTiming{.publish = 5, .send = 6, .delivery = 16},
                          EventSequence{12}};

    const auto values_match =
        message.id() == MessageId{4} && message.source_job() == JobIdentity{TaskId{1}, JobId{7}} &&
        message.destination_task_id() == TaskId{2} && message.publish_tick() == 5 &&
        message.send_tick() == 6 && message.delivery_tick() == 16 &&
        message.publication_sequence() == EventSequence{12} &&
        message.lifecycle() == MessageLifecycle::PendingSend &&
        !message.send_event_sequence().has_value() &&
        !message.delivery_event_sequence().has_value();
    REQUIRE(values_match);
}

/*** Verifies strict positive send and delivery intervals. ***/
TEST_CASE("message construction rejects invalid planned timing", "[model][message]") {
    const auto source = JobIdentity{TaskId{1}, JobId{1}};
    REQUIRE_THROWS_AS(
        (Message{MessageId{1}, source, TaskId{2},
                 MessageTiming{.publish = -1, .send = 1, .delivery = 2}, EventSequence{0}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        (Message{MessageId{1}, source, TaskId{2},
                 MessageTiming{.publish = 1, .send = 1, .delivery = 2}, EventSequence{0}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        (Message{MessageId{1}, source, TaskId{2},
                 MessageTiming{.publish = 1, .send = 2, .delivery = 2}, EventSequence{0}}),
        std::invalid_argument);
}

} // namespace
