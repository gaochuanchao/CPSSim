/***
 * File: src/cpssim/functional/mock_functional_model.cpp
 * Purpose: Implement a small deterministic model whose state exposes generic
 *          runtime and replay call ordering without FMI or Bosch behavior.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/functional/mock_functional_model.hpp"

#include <cmath>
#include <stdexcept>

namespace cpssim {

/*** Rejects a nonfinite starting value that could not enter a valid trace. ***/
MockFunctionalModel::MockFunctionalModel(double initial_state) : state_{initial_state} {
    if (!std::isfinite(state_)) {
        throw std::invalid_argument{"mock functional state must be finite"};
    }
}

/*** Validates direct use as well as normal FunctionalRuntime-mediated use. ***/
FunctionalObservation MockFunctionalModel::initialize(PhysicalDuration tick_period,
                                                      Tick stop_tick) {
    if (initialized_) {
        throw std::logic_error{"mock functional model can initialize only once"};
    }
    if (tick_period.count() <= 0 || stop_tick < 0) {
        throw std::invalid_argument{"mock functional run bounds are invalid"};
    }
    stop_tick_ = stop_tick;
    initialized_ = true;
    return observation();
}

/*** Returns the current state using one stable generic signal name. ***/
FunctionalObservation MockFunctionalModel::observation() const {
    return {.tick = current_tick_,
            .real_signals = {{.name = "mock_state", .value = state_}},
            .integer_signals = {},
            .boolean_signals = {}};
}

/*** Applies pending action count once, then emits every intermediate row. ***/
std::vector<FunctionalObservation> MockFunctionalModel::advance_to(Tick target_tick) {
    if (!initialized_ || finalized_) {
        throw std::logic_error{"mock functional advance requires an active run"};
    }
    if (target_tick < current_tick_ || target_tick > stop_tick_) {
        throw std::invalid_argument{"mock functional target is outside valid progression"};
    }

    std::vector<FunctionalObservation> observations;
    while (current_tick_ < target_tick) {
        state_ += static_cast<double>(pending_action_count_);
        pending_action_count_ = 0;
        ++current_tick_;
        observations.push_back(observation());
    }
    return observations;
}

/*** Stores only the batch size; Event semantics remain the caller's concern. ***/
void MockFunctionalModel::apply_actions(Tick tick, const std::vector<Event>& actions) {
    if (!initialized_ || finalized_ || tick != current_tick_) {
        throw std::logic_error{"mock functional action uses invalid lifecycle or tick"};
    }
    pending_action_count_ = actions.size();
}

/*** Requires the configured final observation before becoming terminal. ***/
void MockFunctionalModel::finalize() {
    if (!initialized_ || finalized_ || current_tick_ != stop_tick_) {
        throw std::logic_error{"mock functional model cannot finalize in its current state"};
    }
    finalized_ = true;
}

} // namespace cpssim
