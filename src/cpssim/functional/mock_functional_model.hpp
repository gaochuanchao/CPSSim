/***
 * File: src/cpssim/functional/mock_functional_model.hpp
 * Purpose: Declare a deterministic dependency-free functional model for
 *          engine, policy, lifecycle, and replay tests.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Each applied event increments mock_state over the next tick only.
 ***/

#pragma once

#include "cpssim/functional/functional_model.hpp"

#include <vector>

namespace cpssim {

/***
 * Implements the generic interface without an external runtime. Its simple
 * state rule makes observe-before-action ordering visible in small tests.
 ***/
class MockFunctionalModel : public FunctionalModel {
  public:
    // Creates an uninitialized model with the supplied finite initial state.
    MockFunctionalModel(double initial_state = 0.0);

    // Starts the mock and returns its initial tick-zero state.
    FunctionalObservation initialize(PhysicalDuration tick_period, Tick stop_tick) override;

    /***
     * Advances consecutively; pending actions affect only the first following
     * tick, after which the mock holds its state.
     ***/
    std::vector<FunctionalObservation> advance_to(Tick target_tick) override;

    // Counts the accepted events that will affect the next tick.
    void apply_actions(Tick tick, const std::vector<Event>& actions) override;

    // Marks the model finalized after its configured stop tick.
    void finalize() override;

    // Reports whether finalize completed successfully.
    bool finalized() const { return finalized_; }

  private:
    // Creates the one-signal observation for the current mock state.
    FunctionalObservation observation() const;

    double state_;
    Tick stop_tick_{0};
    Tick current_tick_{0};
    std::size_t pending_action_count_{0};
    bool initialized_{false};
    bool finalized_{false};
};

} // namespace cpssim
