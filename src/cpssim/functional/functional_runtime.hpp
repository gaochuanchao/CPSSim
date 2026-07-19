/***
 * File: src/cpssim/functional/functional_runtime.hpp
 * Purpose: Declare validated online functional orchestration, append-only
 *          observation trace ownership, and canonical offline replay.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: ADR-0017 defines the observe-before-same-tick-action contract.
 ***/

#pragma once

#include "cpssim/functional/functional_model.hpp"

#include <vector>

namespace cpssim {

/***
 * Coordinates one non-owned FunctionalModel over an inclusive integer horizon.
 * The model must outlive this object. Runtime validation prevents lifecycle,
 * tick, observation-shape, and repeated-action ambiguity.
 ***/
class FunctionalRuntime {
  public:
    /***
     * Stores the model reference and validates positive tick duration and a
     * nonnegative stop tick. No external-model call occurs during construction.
     ***/
    FunctionalRuntime(FunctionalModel& model, PhysicalDuration tick_period, Tick stop_tick);

    // Initializes once, validates the tick-zero row, and appends it.
    std::vector<FunctionalObservation> initialize();

    /***
     * Advances monotonically within the horizon and appends exactly one row
     * for every newly completed integer tick.
     ***/
    std::vector<FunctionalObservation> advance_to(Tick target_tick);

    /***
     * Applies at most one batch at the current tick. Every event must use that
     * tick; the event order is preserved for the adapter.
     ***/
    void apply_actions(Tick tick, const std::vector<Event>& actions);

    // Finalizes once and only after the inclusive stop tick has been observed.
    void finalize();

    // Returns the runtime's current canonical tick.
    Tick current_tick() const { return current_tick_; }

    // Returns the validated append-only functional observation history.
    const std::vector<FunctionalObservation>& trace() const { return trace_; }

  private:
    FunctionalModel& model_;
    PhysicalDuration tick_period_;
    Tick stop_tick_;
    Tick current_tick_{0};
    bool initialized_{false};
    bool finalized_{false};
    bool actions_applied_at_current_tick_{false};
    std::vector<FunctionalObservation> trace_;
};

/***
 * Replays a nondecreasing canonical event trace through one fresh model using
 * the same ordering as online execution. Returns the completed functional
 * trace and rejects events outside the inclusive horizon.
 ***/
std::vector<FunctionalObservation>
replay_functional_trace(FunctionalModel& model, PhysicalDuration tick_period, Tick stop_tick,
                        const std::vector<Event>& canonical_trace);

} // namespace cpssim
