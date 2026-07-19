/***
 * File: src/cpssim/bosch/trigger_encoder.cpp
 * Purpose: Project generic CPSSim events onto the sixteen Boolean trigger
 *          inputs used by the Bosch v10 FMU and export sparse trigger CSV.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This adapter owns every Bosch-specific task and trigger mapping. The
 *        simulator core remains independent of the Bosch FMU vocabulary.
 ***/

#include "cpssim/bosch/trigger_encoder.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace cpssim {
namespace {

/*** Maps a Bosch task number to its activation trigger. ***/
BoschTrigger activation_trigger(TaskId task_id) {
    switch (task_id.value()) {
    case 1:
        return BoschTrigger::SensorActivated;
    case 2:
        return BoschTrigger::EstimatorActivated;
    case 3:
        return BoschTrigger::ControllerActivated;
    case 4:
        return BoschTrigger::FeedforwardActivated;
    case 5:
        return BoschTrigger::MergerActivated;
    case 6:
        return BoschTrigger::ActuatorActivated;
    default:
        throw std::logic_error{"job event refers to a task outside the Bosch v10 mapping"};
    }
}

/*** Maps a Bosch task number to its completion trigger. ***/
BoschTrigger finish_trigger(TaskId task_id) {
    switch (task_id.value()) {
    case 1:
        return BoschTrigger::SensorFinished;
    case 2:
        return BoschTrigger::EstimatorFinished;
    case 3:
        return BoschTrigger::ControllerFinished;
    case 4:
        return BoschTrigger::FeedforwardFinished;
    case 5:
        return BoschTrigger::MergerFinished;
    case 6:
        return BoschTrigger::ActuatorFinished;
    default:
        throw std::logic_error{"job event refers to a task outside the Bosch v10 mapping"};
    }
}

/*** Maps the message-producing task to its Bosch send trigger. ***/
BoschTrigger send_trigger(TaskId task_id) {
    switch (task_id.value()) {
    case 1:
        return BoschTrigger::UplinkSent;
    case 5:
        return BoschTrigger::DownlinkSent;
    default:
        throw std::logic_error{"message-send event refers to an unsupported Bosch v10 task"};
    }
}

/*** Maps the message-consuming task to its Bosch delivery trigger. ***/
BoschTrigger delivery_trigger(TaskId task_id) {
    switch (task_id.value()) {
    case 2:
        return BoschTrigger::UplinkReceived;
    case 6:
        return BoschTrigger::DownlinkReceived;
    default:
        throw std::logic_error{"message-delivery event refers to an unsupported Bosch v10 task"};
    }
}

/*** Returns the required task reference from an event selected for mapping. ***/
TaskId required_task_id(const Event& event) {
    const auto& task_id = event.entities().task_id;
    if (!task_id.has_value()) {
        throw std::logic_error{"mapped Bosch event is missing its task reference"};
    }
    return task_id.value();
}

/*** Verifies that a mapped event occurs in its canonical processing phase. ***/
void require_phase(const Event& event, EventPhase expected) {
    if (event.phase() != expected) {
        throw std::logic_error{"mapped Bosch event has an inconsistent canonical phase"};
    }
}

/*** Sorts pulses by tick and external column, then applies Boolean deduplication. ***/
void normalize(std::vector<BoschTriggerEvent>& events) {
    std::sort(events.begin(), events.end(), [](const auto& left, const auto& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        return bosch_trigger_column(left.trigger) < bosch_trigger_column(right.trigger);
    });

    const auto duplicate_begin =
        std::unique(events.begin(), events.end(), [](const auto& left, const auto& right) {
            return left.tick == right.tick && left.trigger == right.trigger;
        });
    events.erase(duplicate_begin, events.end());
}

/*** Formats Bosch v10 seconds exactly from a 0.0001-second integer tick. ***/
std::string seconds_text(Tick tick) {
    if (tick < 0) {
        throw std::invalid_argument{"Bosch trigger CSV cannot contain a negative tick"};
    }

    constexpr Tick ticks_per_second = 10000;
    const Tick seconds = tick / ticks_per_second;
    const Tick fractional_ticks = tick % ticks_per_second;
    if (fractional_ticks == 0) {
        return std::to_string(seconds);
    }

    std::string fraction = std::to_string(fractional_ticks);
    fraction.insert(0, 4 - fraction.size(), '0');
    while (fraction.back() == '0') {
        fraction.pop_back();
    }
    return std::to_string(seconds) + "." + fraction;
}

} // namespace

std::size_t bosch_trigger_column(BoschTrigger trigger) {
    switch (trigger) {
    case BoschTrigger::SensorActivated:
        return 1;
    case BoschTrigger::SensorFinished:
        return 2;
    case BoschTrigger::UplinkSent:
        return 3;
    case BoschTrigger::UplinkReceived:
        return 4;
    case BoschTrigger::EstimatorActivated:
        return 5;
    case BoschTrigger::EstimatorFinished:
        return 6;
    case BoschTrigger::ControllerActivated:
        return 7;
    case BoschTrigger::ControllerFinished:
        return 8;
    case BoschTrigger::FeedforwardActivated:
        return 9;
    case BoschTrigger::FeedforwardFinished:
        return 10;
    case BoschTrigger::MergerActivated:
        return 11;
    case BoschTrigger::MergerFinished:
        return 12;
    case BoschTrigger::DownlinkSent:
        return 13;
    case BoschTrigger::DownlinkReceived:
        return 14;
    case BoschTrigger::ActuatorActivated:
        return 15;
    case BoschTrigger::ActuatorFinished:
        return 16;
    }
    throw std::logic_error{"unknown Bosch trigger"};
}

std::string_view bosch_trigger_name(BoschTrigger trigger) {
    switch (trigger) {
    case BoschTrigger::SensorActivated:
        return "sensor_activated";
    case BoschTrigger::SensorFinished:
        return "sensor_finished";
    case BoschTrigger::UplinkSent:
        return "uplink_sent";
    case BoschTrigger::UplinkReceived:
        return "uplink_received";
    case BoschTrigger::EstimatorActivated:
        return "estimator_activated";
    case BoschTrigger::EstimatorFinished:
        return "estimator_finished";
    case BoschTrigger::ControllerActivated:
        return "controller_activated";
    case BoschTrigger::ControllerFinished:
        return "controller_finished";
    case BoschTrigger::FeedforwardActivated:
        return "feedforward_activated";
    case BoschTrigger::FeedforwardFinished:
        return "feedforward_finished";
    case BoschTrigger::MergerActivated:
        return "merger_activated";
    case BoschTrigger::MergerFinished:
        return "merger_finished";
    case BoschTrigger::DownlinkSent:
        return "downlink_sent";
    case BoschTrigger::DownlinkReceived:
        return "downlink_received";
    case BoschTrigger::ActuatorActivated:
        return "actuator_activated";
    case BoschTrigger::ActuatorFinished:
        return "actuator_finished";
    }
    throw std::logic_error{"unknown Bosch trigger"};
}

std::vector<BoschTriggerEvent> encode_bosch_v10_triggers(const std::vector<Event>& trace) {
    std::vector<BoschTriggerEvent> encoded;

    for (const auto& event : trace) {
        switch (event.type()) {
        case EventType::JobStart:
            require_phase(event, EventPhase::Scheduling);
            encoded.push_back({event.tick(), activation_trigger(required_task_id(event))});
            break;
        case EventType::JobFinish:
            require_phase(event, EventPhase::ExecutionCompletion);
            encoded.push_back({event.tick(), finish_trigger(required_task_id(event))});
            break;
        case EventType::MessageSend:
            require_phase(event, EventPhase::CausedAction);
            encoded.push_back({event.tick(), send_trigger(required_task_id(event))});
            break;
        case EventType::MessageDelivery:
            require_phase(event, EventPhase::MessageDelivery);
            encoded.push_back({event.tick(), delivery_trigger(required_task_id(event))});
            break;
        case EventType::JobRelease:
        case EventType::JobPreempt:
        case EventType::JobResume:
        case EventType::DeadlineMiss:
            break;
        }
    }

    normalize(encoded);
    return encoded;
}

std::string serialize_bosch_v10_trigger_csv(const std::vector<BoschTriggerEvent>& events) {
    auto normalized = events;
    normalize(normalized);

    std::string csv = "eventTick,eventTimeSec,triggerColumn,triggerName\n";
    for (const auto& event : normalized) {
        csv += std::to_string(event.tick) + "," + seconds_text(event.tick) + "," +
               std::to_string(bosch_trigger_column(event.trigger)) + "," +
               std::string{bosch_trigger_name(event.trigger)} + "\n";
    }
    return csv;
}

} // namespace cpssim
