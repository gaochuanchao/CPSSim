/***
 * File: src/cpssim/model/event.hpp
 * Purpose: Declare the immutable canonical event record and its typed optional
 *          references to simulator entities.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Event records describe observations; they do not mutate runtime
 *        state. EventQueue owns sequence assignment and queue ordering.
 ***/

#pragma once

#include "cpssim/model/categories.hpp"
#include "cpssim/model/identifiers.hpp"
#include "cpssim/model/time.hpp"

#include <optional>

namespace cpssim {

/***
 * Groups the stable entity identifiers that may be relevant to one event.
 * A field is absent when that entity domain does not apply to the event.
 ***/
struct EventEntityRefs {
    std::optional<TaskId> task_id;
    std::optional<JobId> job_id;
    std::optional<ResourceId> resource_id;
    std::optional<MessageId> message_id;
    std::optional<VehicleId> vehicle_id;
};

/***
 * Stores one immutable, appendable canonical event observation.
 * The constructor validates local time and causality invariants but does not
 * assign a sequence, choose a phase, or modify model state.
 ***/
class Event {
  public:
    /***
     * Creates a canonical event from an already assigned sequence and phase.
     * Throws std::invalid_argument for a negative tick or a cause sequence
     * that is not earlier than this event's sequence.
     ***/
    Event(Tick tick, EventPhase phase, EventSequence sequence, EventType type,
          EventEntityRefs entities = {},
          std::optional<EventSequence> cause_sequence = std::nullopt);

    // Returns the canonical logical event time.
    Tick tick() const { return tick_; }

    // Returns the event's semantic processing phase.
    EventPhase phase() const { return phase_; }

    // Returns the stable insertion sequence assigned to this event.
    EventSequence sequence() const { return sequence_; }

    // Returns the observable event category.
    EventType type() const { return type_; }

    // Returns the event's typed optional entity references.
    const EventEntityRefs& entities() const { return entities_; }

    // Returns the causal predecessor sequence when one was recorded.
    std::optional<EventSequence> cause_sequence() const { return cause_sequence_; }

  private:
    Tick tick_;
    EventPhase phase_;
    EventSequence sequence_;
    EventType type_;
    EventEntityRefs entities_;
    std::optional<EventSequence> cause_sequence_;
};

} // namespace cpssim
