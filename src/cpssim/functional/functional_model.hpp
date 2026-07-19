/***
 * File: src/cpssim/functional/functional_model.hpp
 * Purpose: Define the simulator-independent functional-model interface and
 *          typed, named observations returned to the timing engine.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This core interface contains no FMI, Bosch, MATLAB, or GUI types.
 ***/

#pragma once

#include "cpssim/model/event.hpp"
#include "cpssim/model/time.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cpssim {

/*** Stores one named floating-point observation at an external boundary. ***/
struct FunctionalRealSignal {
    std::string name;
    double value;
};

/*** Stores one named integral observation without converting it to Real. ***/
struct FunctionalIntegerSignal {
    std::string name;
    std::int64_t value;
};

/*** Stores one named logical observation without numeric flattening. ***/
struct FunctionalBooleanSignal {
    std::string name;
    bool value;
};

/***
 * Stores the complete functional observation sampled at one canonical tick.
 * Signal vectors retain adapter-defined stable order for deterministic traces.
 ***/
struct FunctionalObservation {
    Tick tick;
    std::vector<FunctionalRealSignal> real_signals;
    std::vector<FunctionalIntegerSignal> integer_signals;
    std::vector<FunctionalBooleanSignal> boolean_signals;
};

/***
 * Defines the replaceable physical/functional model driven by the engine.
 * Implementations own external state and environment inputs, while callers
 * own the model object's lifetime and all canonical scheduling state.
 ***/
class FunctionalModel {
  public:
    // Enables safe destruction through the runtime-polymorphic interface.
    virtual ~FunctionalModel() = default;

    /***
     * Starts one run and returns its initialized observation at tick zero.
     * Throws when configuration or external-model initialization fails.
     ***/
    virtual FunctionalObservation initialize(PhysicalDuration tick_period, Tick stop_tick) = 0;

    /***
     * Advances from the implementation's current tick through target_tick.
     * Returns one observation for every newly completed integer tick in order.
     ***/
    virtual std::vector<FunctionalObservation> advance_to(Tick target_tick) = 0;

    /***
     * Applies one ordered accepted-event batch at the current tick. Actions at
     * tick t affect the following interval and observation t + 1.
     ***/
    virtual void apply_actions(Tick tick, const std::vector<Event>& actions) = 0;

    // Finalizes the external model after the inclusive stop tick is observed.
    virtual void finalize() = 0;
};

} // namespace cpssim
