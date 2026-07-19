/***
 * File: src/cpssim/bosch/trigger_encoder.hpp
 * Purpose: Declare the Bosch v10 sixteen-trigger vocabulary, sparse event
 *          projection, and deterministic CSV exporter.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This is an adapter interface. cpssim_core contains no Bosch trigger
 *        names, columns, task mapping, or FMI value references.
 ***/

#pragma once

#include "cpssim/model/event.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

/*** Names the sixteen Boolean inputs expected by the Bosch v10 FMU. ***/
enum class BoschTrigger {
    SensorActivated,
    SensorFinished,
    UplinkSent,
    UplinkReceived,
    EstimatorActivated,
    EstimatorFinished,
    ControllerActivated,
    ControllerFinished,
    FeedforwardActivated,
    FeedforwardFinished,
    MergerActivated,
    MergerFinished,
    DownlinkSent,
    DownlinkReceived,
    ActuatorActivated,
    ActuatorFinished,
};

/*** Stores one true Bosch Boolean input at one canonical integer tick. ***/
struct BoschTriggerEvent {
    Tick tick;
    BoschTrigger trigger;
};

// Returns the fixed one-based FMU trigger column.
std::size_t bosch_trigger_column(BoschTrigger trigger);

// Returns the stable captured-CSV name for one trigger.
std::string_view bosch_trigger_name(BoschTrigger trigger);

/***
 * Projects accepted generic events into sorted, duplicate-free Bosch pulses.
 * Throws std::logic_error when a mapped event has an inconsistent phase,
 * missing task reference, or task outside the v10 mapping.
 ***/
std::vector<BoschTriggerEvent> encode_bosch_v10_triggers(const std::vector<Event>& trace);

/***
 * Serializes sparse events using the captured four-column trigger CSV schema.
 * Seconds are formatted exactly from the fixed 0.0001-second v10 tick.
 * Throws std::invalid_argument when a caller supplies a negative tick.
 ***/
std::string serialize_bosch_v10_trigger_csv(const std::vector<BoschTriggerEvent>& events);

} // namespace cpssim
