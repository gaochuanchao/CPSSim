/***
 * File: src/cpssim/kernel/event_queue.cpp
 * Purpose: Implement deterministic event insertion, phase precedence, and
 *          removal from the pending-event queue.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Ordering is explicit and does not depend on enum numeric values or
 *        standard-container iteration order.
 ***/

#include "cpssim/kernel/event_queue.hpp"

#include <limits>
#include <stdexcept>

namespace cpssim {
namespace {

/***
 * Maps semantic phases to the accepted same-tick precedence from ADR-0004.
 * Lower values are processed first; every enum value is named explicitly so
 * declaration-order changes cannot alter queue behavior.
 ***/
int phase_precedence(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return 0;
    case EventPhase::MessageDelivery:
        return 1;
    case EventPhase::DeadlineCheck:
        return 2;
    case EventPhase::JobRelease:
        return 3;
    case EventPhase::PolicyUpdate:
        return 4;
    case EventPhase::Scheduling:
        return 5;
    case EventPhase::CausedAction:
        return 6;
    }

    throw std::logic_error{"event phase has no queue precedence"};
}

} // namespace

/***
 * Compares tick first, explicit phase precedence second, and insertion
 * sequence last. Returning true for a later left event keeps the earliest
 * event at std::priority_queue::top.
 ***/
bool EventQueue::LaterEvent::operator()(const Event& left, const Event& right) const {
    if (left.tick() != right.tick()) {
        return left.tick() > right.tick();
    }

    const auto left_phase = phase_precedence(left.phase());
    const auto right_phase = phase_precedence(right.phase());
    if (left_phase != right_phase) {
        return left_phase > right_phase;
    }

    return left.sequence() > right.sequence();
}

/***
 * Constructs an Event before modifying queue state, inserts it, and advances
 * the allocator only after insertion succeeds. The maximum unsigned sequence
 * is usable once, after which later scheduling reports exhaustion.
 ***/
EventSequence EventQueue::schedule(Tick tick, EventPhase phase, EventType type,
                                   EventEntityRefs entities,
                                   std::optional<EventSequence> cause_sequence) {
    if (sequence_space_exhausted_) {
        throw std::overflow_error{"event sequence domain is exhausted"};
    }

    // Reject an unsafe enum value before it can enter the heap.
    phase_precedence(phase);
    const Event event{tick, phase, EventSequence{next_sequence_}, type, entities, cause_sequence};
    events_.push(event);

    const auto assigned_sequence = event.sequence();
    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        sequence_space_exhausted_ = true;
    } else {
        ++next_sequence_;
    }

    return assigned_sequence;
}

/*** Checks the empty precondition before exposing the next owned event. ***/
const Event& EventQueue::next() const {
    if (events_.empty()) {
        throw std::out_of_range{"cannot inspect an empty event queue"};
    }
    return events_.top();
}

/*** Copies the earliest event, removes the owned queue entry, and returns it. ***/
Event EventQueue::pop_next() {
    if (events_.empty()) {
        throw std::out_of_range{"cannot pop an empty event queue"};
    }

    const Event event = events_.top();
    events_.pop();
    return event;
}

} // namespace cpssim
