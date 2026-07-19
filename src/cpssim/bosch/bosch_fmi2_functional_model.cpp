/***
 * File: src/cpssim/bosch/bosch_fmi2_functional_model.cpp
 * Purpose: Drive the Bosch v10 FMI model from canonical event batches,
 *          integer-derived time, and immutable feedforward/velocity samples.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Actions at tick t affect the FMI step [t, t + 1), as fixed by
 *        ADR-0017. Trigger value references are mapped explicitly.
 ***/

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"

#include "cpssim/bosch/trigger_encoder.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace cpssim {
namespace {

constexpr PhysicalDuration bosch_tick_period = std::chrono::microseconds{100};

/*** Returns the explicit FMI Boolean value reference for one Bosch trigger. ***/
Fmi2ValueReference trigger_reference(BoschTrigger trigger) {
    switch (trigger) {
    case BoschTrigger::SensorActivated:
        return 100;
    case BoschTrigger::SensorFinished:
        return 101;
    case BoschTrigger::UplinkSent:
        return 102;
    case BoschTrigger::UplinkReceived:
        return 103;
    case BoschTrigger::EstimatorActivated:
        return 104;
    case BoschTrigger::EstimatorFinished:
        return 105;
    case BoschTrigger::ControllerActivated:
        return 106;
    case BoschTrigger::ControllerFinished:
        return 107;
    case BoschTrigger::FeedforwardActivated:
        return 108;
    case BoschTrigger::FeedforwardFinished:
        return 109;
    case BoschTrigger::MergerActivated:
        return 110;
    case BoschTrigger::MergerFinished:
        return 111;
    case BoschTrigger::DownlinkSent:
        return 112;
    case BoschTrigger::DownlinkReceived:
        return 113;
    case BoschTrigger::ActuatorActivated:
        return 114;
    case BoschTrigger::ActuatorFinished:
        return 115;
    }
    throw std::logic_error{"unknown Bosch trigger value reference"};
}

/*** Converts exact integer nanoseconds to the FMI-required seconds value. ***/
double seconds_at(Tick tick, PhysicalDuration tick_period) {
    const auto duration = ticks_to_duration(tick, tick_period);
    return static_cast<double>(duration.count()) / 1'000'000'000.0;
}

/*** Converts any unsuccessful FMI call into a contextual runtime failure. ***/
void require_success(const Fmi2CallResult& result) {
    if (!result.succeeded()) {
        throw std::runtime_error{"Bosch functional FMI call failed: " + result.message};
    }
}

/***
 * Returns every Bosch v10 parameter value configured by the supplied Simulink
 * block. The FMU C source zero-initializes storage at instantiation; FMI start
 * attributes are metadata and must be applied by the importing master.
 ***/
std::vector<Fmi2InitialReal> bosch_initial_values(double initial_velocity) {
    return {{.reference = 1, .value = -0.987},  {.reference = 2, .value = 0.0},
            {.reference = 3, .value = 0.0},     {.reference = 4, .value = 0.0},
            {.reference = 5, .value = 8.0},     {.reference = 6, .value = 1.75},
            {.reference = 7, .value = 1.987},   {.reference = 8, .value = 0.1974},
            {.reference = 9, .value = -0.2876}, {.reference = 10, .value = 0.0},
            {.reference = 11, .value = 1.0},    {.reference = 12, .value = 0.0},
            {.reference = 13, .value = 0.0},    {.reference = 14, .value = 0.0},
            {.reference = 15, .value = 0.2876}, {.reference = 16, .value = 0.0},
            {.reference = 17, .value = 0.0},    {.reference = 18, .value = 0.0},
            {.reference = 19, .value = 0.0},    {.reference = 20, .value = 0.1},
            {.reference = 21, .value = 0.0},    {.reference = 22, .value = 0.0},
            {.reference = 23, .value = 0.0},    {.reference = 24, .value = 0.0},
            {.reference = 25, .value = 0.0},    {.reference = 26, .value = 0.0},
            {.reference = 27, .value = 0.0},    {.reference = 28, .value = 0.0},
            {.reference = 29, .value = 0.0},    {.reference = 30, .value = 0.0},
            {.reference = 31, .value = 0.0},    {.reference = 32, .value = 0.0},
            {.reference = 33, .value = 0.0},    {.reference = 34, .value = initial_velocity}};
}

} // namespace

/*** Loads the library first, then validates every copied environment sample. ***/
BoschFmi2FunctionalModel::BoschFmi2FunctionalModel(const Fmi2ModelInfo& model,
                                                   std::vector<BoschTrajectorySample> trajectory)
    : fmu_{model}, trajectory_{std::move(trajectory)} {
    if (trajectory_.empty()) {
        throw std::invalid_argument{"Bosch functional trajectory must not be empty"};
    }
    if (trajectory_.size() > static_cast<std::uintmax_t>(std::numeric_limits<Tick>::max())) {
        throw std::invalid_argument{"Bosch functional trajectory exceeds Tick indexing"};
    }
    for (const auto& sample : trajectory_) {
        if (!std::isfinite(sample.feedforward_0) || !std::isfinite(sample.feedforward_1) ||
            !std::isfinite(sample.velocity) || sample.velocity <= 0.0) {
            throw std::invalid_argument{"Bosch trajectory requires finite values and velocity > 0"};
        }
    }
}

/*** Applies the initial lateral error and velocity inside FMI initialization mode. ***/
FunctionalObservation BoschFmi2FunctionalModel::initialize(PhysicalDuration tick_period,
                                                           Tick stop_tick) {
    if (initialized_) {
        throw std::logic_error{"Bosch functional model can initialize only once"};
    }
    if (tick_period != bosch_tick_period) {
        throw std::invalid_argument{"Bosch v10 functional model requires a 100 microsecond tick"};
    }
    if (stop_tick < 0 || stop_tick >= static_cast<Tick>(trajectory_.size())) {
        throw std::invalid_argument{"Bosch trajectory does not cover the inclusive stop tick"};
    }
    if (stop_tick > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument{"Bosch FMU step output cannot represent the stop tick"};
    }

    tick_period_ = tick_period;
    stop_tick_ = stop_tick;
    const auto initial_values = bosch_initial_values(trajectory_.front().velocity);
    require_success(fmu_.initialize(0.0, seconds_at(stop_tick_, tick_period_), initial_values));
    initialized_ = true;
    return observation();
}

/*** Applies each interval's inputs, step, pulse reset, and output sampling. ***/
std::vector<FunctionalObservation> BoschFmi2FunctionalModel::advance_to(Tick target_tick) {
    if (!initialized_ || finalized_) {
        throw std::logic_error{"Bosch functional advance requires an active FMI component"};
    }
    if (target_tick < current_tick_ || target_tick > stop_tick_) {
        throw std::invalid_argument{"Bosch functional target is outside valid progression"};
    }

    std::vector<FunctionalObservation> observations;
    while (current_tick_ < target_tick) {
        const auto& input = trajectory_[static_cast<std::size_t>(current_tick_)];
        require_success(fmu_.set_real(116, input.feedforward_0));
        require_success(fmu_.set_real(117, input.feedforward_1));
        require_success(fmu_.set_real(118, input.velocity));
        require_success(
            fmu_.do_step(seconds_at(current_tick_, tick_period_), seconds_at(1, tick_period_)));

        for (const auto reference : active_trigger_references_) {
            require_success(fmu_.set_boolean(reference, false));
        }
        active_trigger_references_.clear();
        ++current_tick_;
        observations.push_back(observation());
    }
    return observations;
}

/*** Encodes the complete batch before writing any deduplicated true pulse. ***/
void BoschFmi2FunctionalModel::apply_actions(Tick tick, const std::vector<Event>& actions) {
    if (!initialized_ || finalized_ || tick != current_tick_) {
        throw std::logic_error{"Bosch functional action uses invalid lifecycle or tick"};
    }
    const auto triggers = encode_bosch_v10_triggers(actions);
    for (const auto& trigger : triggers) {
        if (trigger.tick != tick) {
            throw std::logic_error{"Bosch trigger differs from its functional action tick"};
        }
        const auto reference = trigger_reference(trigger.trigger);
        require_success(fmu_.set_boolean(reference, true));
        active_trigger_references_.push_back(reference);
    }
}

/*** Reads typed selected outputs and verifies the FMU's own time/step state. ***/
FunctionalObservation BoschFmi2FunctionalModel::observation() {
    double lateral_error = 0.0;
    double actuator_command = 0.0;
    double rolling_real = 0.0;
    double rolling_remote = 0.0;
    std::int32_t violation_counter = 0;
    bool critical_section = false;
    double fmu_time = 0.0;
    std::int32_t fmu_step = 0;

    require_success(fmu_.get_real(1004, lateral_error));
    require_success(fmu_.get_real(1022, actuator_command));
    require_success(fmu_.get_real(1030, rolling_real));
    require_success(fmu_.get_real(1029, rolling_remote));
    require_success(fmu_.get_integer(1036, violation_counter));
    require_success(fmu_.get_boolean(1027, critical_section));
    require_success(fmu_.get_real(1023, fmu_time));
    require_success(fmu_.get_integer(1024, fmu_step));

    const double expected_time = seconds_at(current_tick_, tick_period_);
    if (std::abs(fmu_time - expected_time) > 1e-8 ||
        fmu_step != static_cast<std::int32_t>(current_tick_)) {
        throw std::runtime_error{"Bosch FMU time or step differs from canonical Tick"};
    }

    return {.tick = current_tick_,
            .real_signals = {{.name = "lateral_error", .value = lateral_error},
                             {.name = "actuator_command", .value = actuator_command},
                             {.name = "rolling_real", .value = rolling_real},
                             {.name = "rolling_remote", .value = rolling_remote}},
            .integer_signals = {{.name = "violation_counter", .value = violation_counter}},
            .boolean_signals = {{.name = "critical_section", .value = critical_section}}};
}

/*** Terminates only after all requested physical steps have completed. ***/
void BoschFmi2FunctionalModel::finalize() {
    if (!initialized_ || finalized_ || current_tick_ != stop_tick_) {
        throw std::logic_error{"Bosch functional model cannot finalize in its current state"};
    }
    require_success(fmu_.terminate());
    finalized_ = true;
}

} // namespace cpssim
