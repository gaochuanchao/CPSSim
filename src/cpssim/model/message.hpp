/***
 * File: src/cpssim/model/message.hpp
 * Purpose: Declare one generic task-routed message with exact planned timing,
 *          causal event identities, and validated transmission lifecycle.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: FixedDelayNetwork owns and changes Message values. The record contains
 *        no Bosch trigger, payload, FMI, server, or resource assumptions.
 ***/

#pragma once

#include "cpssim/model/categories.hpp"
#include "cpssim/model/identifiers.hpp"
#include "cpssim/model/time.hpp"

#include <optional>

namespace cpssim {

class FixedDelayNetwork;

/*** Groups publication, planned send, and planned delivery integer ticks. ***/
struct MessageTiming {
    Tick publish;
    Tick send;
    Tick delivery;
};

/*** Stores one completion-triggered message and its transmission progress. ***/
class Message {
  public:
    /***
     * Creates one PendingSend message. Send must follow publication and
     * delivery must follow send. Throws std::invalid_argument otherwise.
     ***/
    Message(MessageId id, JobIdentity source_job, TaskId destination_task_id, MessageTiming timing,
            EventSequence publication_sequence);

    // Returns the globally unique runtime message ID.
    MessageId id() const { return id_; }

    // Returns the producer job whose completion published this message.
    JobIdentity source_job() const { return source_job_; }

    // Returns the configured receiving task endpoint.
    TaskId destination_task_id() const { return destination_task_id_; }

    // Returns the accepted producer-completion tick.
    Tick publish_tick() const { return timing_.publish; }

    // Returns the planned send tick, whether or not it is in horizon.
    Tick send_tick() const { return timing_.send; }

    // Returns the planned delivery tick, whether or not it is in horizon.
    Tick delivery_tick() const { return timing_.delivery; }

    // Returns the accepted JobFinish event that caused publication.
    EventSequence publication_sequence() const { return publication_sequence_; }

    // Returns the current mutually exclusive transmission state.
    MessageLifecycle lifecycle() const { return lifecycle_; }

    // Returns the scheduled send candidate sequence when it is in horizon.
    std::optional<EventSequence> send_event_sequence() const { return send_event_sequence_; }

    // Returns the scheduled delivery candidate sequence when it is in horizon.
    std::optional<EventSequence> delivery_event_sequence() const {
        return delivery_event_sequence_;
    }

  private:
    friend class FixedDelayNetwork;

    // Records the unique in-horizon MessageSend candidate.
    void set_send_event_sequence(EventSequence sequence);

    // Changes a validated PendingSend message to InFlight.
    void mark_sent();

    // Records the unique in-horizon MessageDelivery candidate.
    void set_delivery_event_sequence(EventSequence sequence);

    // Changes a validated InFlight message to Delivered.
    void mark_delivered();

    MessageId id_;
    JobIdentity source_job_;
    TaskId destination_task_id_;
    MessageTiming timing_;
    EventSequence publication_sequence_;
    MessageLifecycle lifecycle_{MessageLifecycle::PendingSend};
    std::optional<EventSequence> send_event_sequence_;
    std::optional<EventSequence> delivery_event_sequence_;
};

} // namespace cpssim
