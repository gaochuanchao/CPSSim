/***
 * File: src/cpssim/trace/event_json.cpp
 * Purpose: Implement byte-stable JSON Lines serialization for canonical event
 *          records.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: ordered_json preserves the schema field order explicitly. This file
 *        is the only T6 source file that depends on nlohmann/json.
 ***/

#include "cpssim/trace/event_json.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace cpssim {

namespace {

using Json = nlohmann::ordered_json;

/***
 * Maps every supported EventPhase to its stable lowercase JSON spelling.
 * The switch is explicit so enum declaration names do not leak into the trace
 * schema accidentally.
 ***/
std::string_view event_phase_name(EventPhase phase) {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return "execution_completion";
    case EventPhase::MessageDelivery:
        return "message_delivery";
    case EventPhase::DeadlineCheck:
        return "deadline_check";
    case EventPhase::JobRelease:
        return "job_release";
    case EventPhase::PolicyUpdate:
        return "policy_update";
    case EventPhase::Scheduling:
        return "scheduling";
    case EventPhase::CausedAction:
        return "caused_action";
    case EventPhase::CausedActionLate:
        return "caused_action_late";
    }

    throw std::logic_error{"event phase has no JSON representation"};
}

/***
 * Maps every currently supported EventType to its stable lowercase JSON
 * spelling and rejects values outside the declared event vocabulary.
 ***/
std::string_view event_type_name(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return "job_release";
    case EventType::JobStart:
        return "job_start";
    case EventType::JobPreempt:
        return "job_preempt";
    case EventType::JobResume:
        return "job_resume";
    case EventType::JobFinish:
        return "job_finish";
    case EventType::DeadlineMiss:
        return "deadline_miss";
    case EventType::MessageSend:
        return "message_send";
    case EventType::MessageDelivery:
        return "message_delivery";
    }

    throw std::logic_error{"event type has no JSON representation"};
}

/***
 * Writes the fixed entity-reference object in canonical field order, using
 * explicit null values when a typed reference is absent.
 ***/
Json serialize_entities(const EventEntityRefs& entities) {
    Json result = Json::object();

    if (entities.task_id.has_value()) {
        result["task_id"] = entities.task_id->value();
    } else {
        result["task_id"] = nullptr;
    }
    if (entities.job_id.has_value()) {
        result["job_id"] = entities.job_id->value();
    } else {
        result["job_id"] = nullptr;
    }
    if (entities.resource_id.has_value()) {
        result["resource_id"] = entities.resource_id->value();
    } else {
        result["resource_id"] = nullptr;
    }
    if (entities.message_id.has_value()) {
        result["message_id"] = entities.message_id->value();
    } else {
        result["message_id"] = nullptr;
    }
    if (entities.vehicle_id.has_value()) {
        result["vehicle_id"] = entities.vehicle_id->value();
    } else {
        result["vehicle_id"] = nullptr;
    }

    return result;
}

} // namespace

/***
 * Builds the canonical ordered object, writes explicit optional values, dumps
 * compact JSON without locale-dependent formatting, and adds one line ending.
 ***/
std::string serialize_event_json_line(const Event& event) {
    Json result = Json::object();
    result["schema_version"] = 1;
    result["tick"] = event.tick();
    result["phase"] = event_phase_name(event.phase());
    result["sequence"] = event.sequence().value();
    result["type"] = event_type_name(event.type());
    result["entities"] = serialize_entities(event.entities());
    const auto cause_sequence = event.cause_sequence();
    if (cause_sequence.has_value()) {
        result["cause_sequence"] = cause_sequence->value();
    } else {
        result["cause_sequence"] = nullptr;
    }

    return result.dump() + '\n';
}

} // namespace cpssim
