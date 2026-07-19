/***
 * File: src/cpssim/kernel/event_queue.hpp
 * Purpose: Declare the deterministic owner of pending canonical events and
 *          insertion-sequence allocation.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The queue orders existing Event records but does not process them,
 *        mutate runtime state, or own the append-only trace.
 ***/

#pragma once

#include "cpssim/model/event.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <vector>

namespace cpssim {

/***
 * Owns pending events and returns them in deterministic tick, phase, and
 * insertion order. Sequence allocation begins at zero for each queue.
 ***/
class EventQueue {
  public:
    /***
     * Accepts the fields known by an event producer, then creates and inserts
     * the complete Event using the queue's next sequence value. A complete
     * Event is not accepted because its sequence would already be chosen.
     * Returns the assigned sequence so later events can record causality.
     * Throws std::invalid_argument for invalid Event fields and
     * std::overflow_error after the unsigned sequence domain is exhausted.
     * A rejected insertion does not consume a sequence value.
     ***/
    EventSequence schedule(Tick tick, EventPhase phase, EventType type,
                           EventEntityRefs entities = {},
                           std::optional<EventSequence> cause_sequence = std::nullopt);

    // Reports whether no events are pending.
    bool empty() const { return events_.empty(); }

    // Returns the number of pending events.
    std::size_t size() const { return events_.size(); }

    /***
     * Returns a read-only reference to the next event without removing it.
     * The reference remains valid only until the queue is modified.
     * Throws std::out_of_range when the queue is empty.
     ***/
    const Event& next() const;

    /***
     * Removes and returns the next event in canonical queue order.
     * Throws std::out_of_range when the queue is empty.
     ***/
    Event pop_next();

  private:
    /*** Adapts canonical earlier-first ordering to std::priority_queue. ***/
    struct LaterEvent {
        // Reports whether left belongs after right in canonical queue order.
        bool operator()(const Event& left, const Event& right) const;
    };

    std::priority_queue<Event, std::vector<Event>, LaterEvent> events_;
    std::uint64_t next_sequence_{0};
    bool sequence_space_exhausted_{false};
};

} // namespace cpssim
