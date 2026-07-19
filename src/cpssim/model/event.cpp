/***
 * File: src/cpssim/model/event.cpp
 * Purpose: Implement local validation and immutable storage for canonical
 *          event records.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Validation is limited to invariants independent of future queue and
 *        event-production policy.
 ***/

#include "cpssim/model/event.hpp"

#include <stdexcept>

namespace cpssim {

/***
 * Stores the supplied canonical fields after rejecting negative time and any
 * causal sequence that is equal to or later than the event being created.
 ***/
Event::Event(Tick tick, EventPhase phase, EventSequence sequence, EventType type,
             EventEntityRefs entities, std::optional<EventSequence> cause_sequence)
    : tick_{tick}, phase_{phase}, sequence_{sequence}, type_{type}, entities_{entities},
      cause_sequence_{cause_sequence} {
    if (tick_ < 0) {
        throw std::invalid_argument{"event tick must not be negative"};
    }
    if (cause_sequence_.has_value() && cause_sequence_.value() >= sequence_) {
        throw std::invalid_argument{"event cause sequence must precede its sequence"};
    }
}

} // namespace cpssim
