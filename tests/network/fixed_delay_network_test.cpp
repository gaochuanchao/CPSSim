/***
 * File: tests/network/fixed_delay_network_test.cpp
 * Purpose: Verify deterministic route expansion, causal event scheduling,
 *          message lifecycle, horizon handling, and network validation.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/network/fixed_delay_network.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>
#include <stdexcept>

namespace {

using cpssim::EventEntityRefs;
using cpssim::EventPhase;
using cpssim::EventQueue;
using cpssim::EventType;
using cpssim::FixedDelayNetwork;
using cpssim::JobId;
using cpssim::MessageId;
using cpssim::MessageLifecycle;
using cpssim::MessageRouteSpec;
using cpssim::ResourceId;
using cpssim::TaskId;

/*** Creates one canonical accepted-completion fixture. ***/
cpssim::Event make_completion(EventQueue& queue, cpssim::Tick tick = 5) {
    queue.schedule(tick, EventPhase::ExecutionCompletion, EventType::JobFinish,
                   EventEntityRefs{.task_id = TaskId{1},
                                   .job_id = JobId{3},
                                   .resource_id = ResourceId{1},
                                   .message_id = std::nullopt,
                                   .vehicle_id = std::nullopt});
    return queue.pop_next();
}

/*** Verifies route order, one-based IDs, and finish-to-send-to-delivery causes. ***/
TEST_CASE("fixed delay network expands routes in deterministic causal order",
          "[network][causality]") {
    FixedDelayNetwork network{{MessageRouteSpec{.source_task_id = TaskId{1},
                                                .destination_task_id = TaskId{3},
                                                .send_offset = 1,
                                                .delay = 10},
                               MessageRouteSpec{.source_task_id = TaskId{1},
                                                .destination_task_id = TaskId{2},
                                                .send_offset = 1,
                                                .delay = 10}},
                              20};
    EventQueue queue;
    const auto completion = make_completion(queue);

    network.publish(completion, queue);
    const auto message_order_matches = network.messages().size() == 2 &&
                                       network.messages()[0].id() == MessageId{1} &&
                                       network.messages()[0].destination_task_id() == TaskId{2} &&
                                       network.messages()[1].id() == MessageId{2} &&
                                       network.messages()[1].destination_task_id() == TaskId{3};
    REQUIRE(message_order_matches);

    const auto first_send = queue.pop_next();
    const auto second_send = queue.pop_next();
    const auto sends_match = first_send.tick() == 6 &&
                             first_send.type() == EventType::MessageSend &&
                             first_send.cause_sequence() == completion.sequence() &&
                             first_send.entities().message_id == MessageId{1} &&
                             second_send.entities().message_id == MessageId{2};
    REQUIRE(sends_match);

    network.process_send(first_send, queue);
    network.process_send(second_send, queue);
    const auto first_delivery = queue.pop_next();
    const auto second_delivery = queue.pop_next();
    const auto delivery_matches = first_delivery.tick() == 16 &&
                                  first_delivery.type() == EventType::MessageDelivery &&
                                  first_delivery.cause_sequence() == first_send.sequence() &&
                                  first_delivery.entities().task_id == TaskId{2};
    REQUIRE(delivery_matches);

    network.process_delivery(first_delivery);
    network.process_delivery(second_delivery);
    const auto lifecycles_match =
        network.messages()[0].lifecycle() == MessageLifecycle::Delivered &&
        network.messages()[1].lifecycle() == MessageLifecycle::Delivered;
    REQUIRE(lifecycles_match);
}

/*** Verifies that planned out-of-horizon work remains inspectable but unqueued. ***/
TEST_CASE("fixed delay network preserves horizon-truncated message state", "[network][horizon]") {
    const auto route = MessageRouteSpec{.source_task_id = TaskId{1},
                                        .destination_task_id = TaskId{2},
                                        .send_offset = 1,
                                        .delay = 10};

    FixedDelayNetwork pending_network{{route}, 5};
    EventQueue pending_queue;
    pending_network.publish(make_completion(pending_queue), pending_queue);
    REQUIRE(pending_queue.empty());
    const auto pending_state_matches =
        pending_network.messages().front().lifecycle() == MessageLifecycle::PendingSend;
    REQUIRE(pending_state_matches);

    FixedDelayNetwork in_flight_network{{route}, 10};
    EventQueue in_flight_queue;
    in_flight_network.publish(make_completion(in_flight_queue), in_flight_queue);
    const auto send = in_flight_queue.pop_next();
    in_flight_network.process_send(send, in_flight_queue);
    REQUIRE(in_flight_queue.empty());
    const auto in_flight_state_matches =
        in_flight_network.messages().front().lifecycle() == MessageLifecycle::InFlight &&
        in_flight_network.messages().front().delivery_tick() == 16;
    REQUIRE(in_flight_state_matches);
}

/*** Verifies construction and runtime-event rejection boundaries. ***/
TEST_CASE("fixed delay network rejects invalid routes and event progression",
          "[network][validation]") {
    REQUIRE_THROWS_AS((FixedDelayNetwork{{MessageRouteSpec{.source_task_id = TaskId{1},
                                                           .destination_task_id = TaskId{2},
                                                           .send_offset = 0,
                                                           .delay = 1}},
                                         10}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((FixedDelayNetwork{{}, -1}), std::invalid_argument);
    REQUIRE_THROWS_AS((FixedDelayNetwork{{MessageRouteSpec{.source_task_id = TaskId{1},
                                                           .destination_task_id = TaskId{2},
                                                           .send_offset = 1,
                                                           .delay = 0}},
                                         10}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((FixedDelayNetwork{{MessageRouteSpec{.source_task_id = TaskId{1},
                                                           .destination_task_id = TaskId{2},
                                                           .send_offset = 1,
                                                           .delay = 1},
                                          MessageRouteSpec{.source_task_id = TaskId{1},
                                                           .destination_task_id = TaskId{2},
                                                           .send_offset = 2,
                                                           .delay = 2}},
                                         10}),
                      std::invalid_argument);

    FixedDelayNetwork network{{}, 10};
    EventQueue queue;
    const auto completion = make_completion(queue);
    REQUIRE_NOTHROW(network.publish(completion, queue));
    REQUIRE(network.messages().empty());
}

/*** Verifies checked arithmetic before a message or candidate is committed. ***/
TEST_CASE("fixed delay network rejects tick overflow", "[network][validation]") {
    FixedDelayNetwork network{{MessageRouteSpec{.source_task_id = TaskId{1},
                                                .destination_task_id = TaskId{2},
                                                .send_offset = 1,
                                                .delay = 1}},
                              std::numeric_limits<cpssim::Tick>::max()};
    EventQueue queue;
    const auto completion = make_completion(queue, std::numeric_limits<cpssim::Tick>::max());

    REQUIRE_THROWS_AS(network.publish(completion, queue), std::overflow_error);
    REQUIRE(network.messages().empty());
}

} // namespace
