/***
 * File: src/cpssim/bosch/bosch_fmi2_functional_model.hpp
 * Purpose: Declare the Bosch v10 trajectory record and generic functional
 *          model implementation backed by one FMI 2.0 component.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This combined adapter owns Bosch value-reference translation; neither
 *        cpssim_core nor the generic FMI importer contains those assumptions.
 ***/

#pragma once

#include "cpssim/fmi/fmi2_importer.hpp"
#include "cpssim/functional/functional_model.hpp"

#include <vector>

namespace cpssim {

/*** Stores the three environment inputs sampled by the Bosch v10 model. ***/
struct BoschTrajectorySample {
    double feedforward_0;
    double feedforward_1;
    double velocity;
};

/***
 * Translates generic accepted events and integer ticks into the Bosch FMI API.
 * The adapter owns its FMI component and immutable trajectory copy. It returns
 * the six typed signals captured by the reference experiment.
 ***/
class BoschFmi2FunctionalModel : public FunctionalModel {
  public:
    /***
     * Loads the prepared FMI platform library and validates finite trajectory
     * samples with positive velocity. No component is instantiated yet.
     ***/
    BoschFmi2FunctionalModel(const Fmi2ModelInfo& model,
                             std::vector<BoschTrajectorySample> trajectory);

    /***
     * Requires the Bosch 0.0001-second tick and enough samples, applies v10
     * initial parameters, initializes FMI, and samples tick zero.
     ***/
    FunctionalObservation initialize(PhysicalDuration tick_period, Tick stop_tick) override;

    /***
     * Applies trajectory sample t and pending trigger pulses before stepping
     * [t, t + 1), then samples all six outputs at t + 1.
     ***/
    std::vector<FunctionalObservation> advance_to(Tick target_tick) override;

    // Projects the current accepted event batch to one-tick Bosch pulses.
    void apply_actions(Tick tick, const std::vector<Event>& actions) override;

    // Terminates the FMI component after the inclusive stop observation.
    void finalize() override;

  private:
    // Reads and validates the six selected output signals at current_tick_.
    FunctionalObservation observation();

    Fmi2CoSimulation fmu_;
    std::vector<BoschTrajectorySample> trajectory_;
    std::vector<Fmi2ValueReference> active_trigger_references_;
    PhysicalDuration tick_period_{0};
    Tick stop_tick_{0};
    Tick current_tick_{0};
    bool initialized_{false};
    bool finalized_{false};
};

} // namespace cpssim
