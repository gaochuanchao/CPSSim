/***
 * File: src/cpssim/model/message.cpp
 * Purpose: Implement message timing validation and network-owned lifecycle
 *          transitions without exposing mutable message state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Candidate-sequence validation occurs in FixedDelayNetwork before it
 *        invokes these small coordinated transitions.
 ***/

#include "cpssim/model/message.hpp"

#include <stdexcept>

namespace cpssim {

/*** Validates strict positive intervals and initializes PendingSend state. ***/
Message::Message(MessageId id, JobIdentity source_job, TaskId destination_task_id,
                 MessageTiming timing, EventSequence publication_sequence)
    : id_{id}, source_job_{source_job}, destination_task_id_{destination_task_id}, timing_{timing},
      publication_sequence_{publication_sequence} {
    if (timing_.publish < 0) {
        throw std::invalid_argument{"message publication tick must not be negative"};
    }
    if (timing_.send <= timing_.publish) {
        throw std::invalid_argument{"message send tick must follow publication"};
    }
    if (timing_.delivery <= timing_.send) {
        throw std::invalid_argument{"message delivery tick must follow send"};
    }
}

/*** Records exactly one send candidate while the message remains pending. ***/
void Message::set_send_event_sequence(EventSequence sequence) {
    if (send_event_sequence_.has_value()) {
        throw std::logic_error{"message already has a send event candidate"};
    }
    send_event_sequence_ = sequence;
}

/*** Advances the validated message into its in-flight interval. ***/
void Message::mark_sent() {
    if (lifecycle_ != MessageLifecycle::PendingSend) {
        throw std::logic_error{"only a pending message can be sent"};
    }
    lifecycle_ = MessageLifecycle::InFlight;
}

/*** Records exactly one delivery candidate after the message is sent. ***/
void Message::set_delivery_event_sequence(EventSequence sequence) {
    if (delivery_event_sequence_.has_value()) {
        throw std::logic_error{"message already has a delivery event candidate"};
    }
    delivery_event_sequence_ = sequence;
}

/*** Advances the validated in-flight message to its terminal state. ***/
void Message::mark_delivered() {
    if (lifecycle_ != MessageLifecycle::InFlight) {
        throw std::logic_error{"only an in-flight message can be delivered"};
    }
    lifecycle_ = MessageLifecycle::Delivered;
}

} // namespace cpssim
