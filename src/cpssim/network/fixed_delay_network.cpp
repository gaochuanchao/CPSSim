/***
 * File: src/cpssim/network/fixed_delay_network.cpp
 * Purpose: Implement deterministic route expansion and validated causal
 *          message-send and message-delivery event progression.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Positive route timing avoids same-tick fixed-point iteration. Planned
 *        out-of-horizon transitions remain in Message but are not queued.
 ***/

#include "cpssim/network/fixed_delay_network.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>

namespace cpssim {

namespace {

/*** Creates source-oriented entity references for one message send. ***/
EventEntityRefs send_entities(const Message& message) {
    return {.task_id = message.source_job().task_id(),
            .job_id = message.source_job().job_id(),
            .resource_id = std::nullopt,
            .message_id = message.id(),
            .vehicle_id = std::nullopt};
}

/*** Creates destination-oriented entity references for one message delivery. ***/
EventEntityRefs delivery_entities(const Message& message) {
    return {.task_id = message.destination_task_id(),
            .job_id = std::nullopt,
            .resource_id = std::nullopt,
            .message_id = message.id(),
            .vehicle_id = std::nullopt};
}

/*** Adds one positive duration without leaving the Tick domain. ***/
Tick checked_add(Tick tick, Tick duration, const char* context) {
    if (duration <= 0) {
        throw std::invalid_argument{context};
    }
    if (duration > std::numeric_limits<Tick>::max() - tick) {
        throw std::overflow_error{"message timing exceeds Tick range"};
    }
    return tick + duration;
}

} // namespace

/*** Validates and sorts routes so input array order cannot affect message IDs. ***/
FixedDelayNetwork::FixedDelayNetwork(const std::vector<MessageRouteSpec>& routes, Tick stop_tick)
    : routes_{routes}, stop_tick_{stop_tick} {
    if (stop_tick_ < 0) {
        throw std::invalid_argument{"network stop tick must not be negative"};
    }
    std::sort(routes_.begin(), routes_.end(),
              [](const MessageRouteSpec& left, const MessageRouteSpec& right) {
                  if (left.source_task_id != right.source_task_id) {
                      return left.source_task_id < right.source_task_id;
                  }
                  return left.destination_task_id < right.destination_task_id;
              });

    for (std::size_t index = 0; index < routes_.size(); ++index) {
        if (routes_[index].send_offset != message_route_send_offset_ticks) {
            throw std::invalid_argument{
                "message route send offset must equal the fixed one-tick causal offset"};
        }
        if (routes_[index].delay <= 0) {
            throw std::invalid_argument{"message route delay must be positive"};
        }
        if (index > 0 && routes_[index - 1].source_task_id == routes_[index].source_task_id &&
            routes_[index - 1].destination_task_id == routes_[index].destination_task_id) {
            throw std::invalid_argument{"message route endpoint pairs must be unique"};
        }
    }
}

/*** Resolves one mutable message without exposing its private transitions. ***/
Message& FixedDelayNetwork::find_message(MessageId id) {
    for (auto& message : messages_) {
        if (message.id() == id) {
            return message;
        }
    }
    throw std::logic_error{"network event refers to an unknown message"};
}

/*** Allocates one stable online creation identity with exhaustion detection. ***/
MessageId FixedDelayNetwork::allocate_message_id() {
    if (message_id_space_exhausted_) {
        throw std::overflow_error{"message ID domain is exhausted"};
    }

    const auto id = MessageId{next_message_id_};
    if (next_message_id_ == std::numeric_limits<std::uint64_t>::max()) {
        message_id_space_exhausted_ = true;
    } else {
        ++next_message_id_;
    }
    return id;
}

/*** Expands a valid completion into ordered messages and send candidates. ***/
void FixedDelayNetwork::publish(const Event& completion, EventQueue& event_queue) {
    const auto task_id = completion.entities().task_id;
    const auto job_id = completion.entities().job_id;
    if (completion.phase() != EventPhase::ExecutionCompletion ||
        completion.type() != EventType::JobFinish || !task_id.has_value() || !job_id.has_value()) {
        throw std::logic_error{"network publication requires a completed source job event"};
    }

    for (const auto& route : routes_) {
        if (route.source_task_id != *task_id) {
            continue;
        }

        const Tick send_tick = checked_add(completion.tick(), route.send_offset,
                                           "message send offset must be positive");
        const Tick delivery_tick =
            checked_add(send_tick, route.delay, "message delay must be positive");
        messages_.emplace_back(
            allocate_message_id(), JobIdentity{*task_id, *job_id}, route.destination_task_id,
            MessageTiming{
                .publish = completion.tick(), .send = send_tick, .delivery = delivery_tick},
            completion.sequence());
        auto& message = messages_.back();
        if (send_tick <= stop_tick_) {
            const auto send_sequence =
                event_queue.schedule(send_tick, EventPhase::CausedAction, EventType::MessageSend,
                                     send_entities(message), completion.sequence());
            message.set_send_event_sequence(send_sequence);
        }
    }
}

/*** Validates the scheduled send identity before advancing and causing delivery. ***/
void FixedDelayNetwork::process_send(const Event& event, EventQueue& event_queue) {
    const auto message_id = event.entities().message_id;
    if (event.phase() != EventPhase::CausedAction || event.type() != EventType::MessageSend ||
        !message_id.has_value()) {
        throw std::logic_error{"network received an invalid message-send event"};
    }

    auto& message = find_message(*message_id);
    if (message.lifecycle() != MessageLifecycle::PendingSend ||
        event.tick() != message.send_tick() || event.sequence() != message.send_event_sequence() ||
        event.cause_sequence() != message.publication_sequence() ||
        event.entities().task_id != message.source_job().task_id() ||
        event.entities().job_id != message.source_job().job_id()) {
        throw std::logic_error{"message-send event does not match pending network state"};
    }

    message.mark_sent();
    if (message.delivery_tick() <= stop_tick_) {
        const auto delivery_sequence = event_queue.schedule(
            message.delivery_tick(), EventPhase::MessageDelivery, EventType::MessageDelivery,
            delivery_entities(message), event.sequence());
        message.set_delivery_event_sequence(delivery_sequence);
    }
}

/*** Validates the delivery candidate before entering terminal Delivered state. ***/
void FixedDelayNetwork::process_delivery(const Event& event) {
    const auto message_id = event.entities().message_id;
    if (event.phase() != EventPhase::MessageDelivery ||
        event.type() != EventType::MessageDelivery || !message_id.has_value()) {
        throw std::logic_error{"network received an invalid message-delivery event"};
    }

    auto& message = find_message(*message_id);
    if (message.lifecycle() != MessageLifecycle::InFlight ||
        event.tick() != message.delivery_tick() ||
        event.sequence() != message.delivery_event_sequence() ||
        event.cause_sequence() != message.send_event_sequence() ||
        event.entities().task_id != message.destination_task_id() ||
        event.entities().job_id.has_value()) {
        throw std::logic_error{"message-delivery event does not match in-flight network state"};
    }
    message.mark_delivered();
}

} // namespace cpssim
