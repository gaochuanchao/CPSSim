/***
 * File: src/cpssim/network/fixed_delay_network.hpp
 * Purpose: Declare deterministic completion-triggered message creation,
 *          in-flight ownership, and fixed-delay send/delivery progression.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The global EventQueue owns pending candidates. This network model owns
 *        routes, Message values, lifecycle, and MessageId allocation.
 ***/

#pragma once

#include "cpssim/kernel/event_queue.hpp"
#include "cpssim/model/message.hpp"
#include "cpssim/model/specifications.hpp"

#include <cstdint>
#include <vector>

namespace cpssim {

/*** Owns generic messages transmitted over configured fixed-delay task routes. ***/
class FixedDelayNetwork {
  public:
    /***
     * Copies and canonically orders routes. Throws std::invalid_argument for a
     * negative horizon, nonpositive timing, or duplicate endpoint pair.
     ***/
    FixedDelayNetwork(const std::vector<MessageRouteSpec>& routes, Tick stop_tick);

    /***
     * Creates one PendingSend message for every route whose source matches an
     * accepted JobFinish. In-horizon sends are caused by that finish event.
     ***/
    void publish(const Event& completion, EventQueue& event_queue);

    /***
     * Applies one expected MessageSend, marks the message InFlight, and
     * schedules an in-horizon delivery caused by the send.
     ***/
    void process_send(const Event& event, EventQueue& event_queue);

    // Applies one expected MessageDelivery and marks its message Delivered.
    void process_delivery(const Event& event);

    // Returns every network-owned message as a read-only view.
    const std::vector<Message>& messages() const { return messages_; }

    // Returns configured routes in deterministic source/destination order.
    const std::vector<MessageRouteSpec>& routes() const { return routes_; }

  private:
    // Finds a mutable network-owned message by stable ID.
    Message& find_message(MessageId id);

    // Allocates the next one-based MessageId or throws on exhaustion.
    MessageId allocate_message_id();

    std::vector<MessageRouteSpec> routes_;
    std::vector<Message> messages_;
    Tick stop_tick_;
    std::uint64_t next_message_id_{1};
    bool message_id_space_exhausted_{false};
};

} // namespace cpssim
