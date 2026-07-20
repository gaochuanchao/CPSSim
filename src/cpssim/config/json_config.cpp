/***
 * File: src/cpssim/config/json_config.cpp
 * Purpose: Parse versioned JSON configuration and translate it into
 *          portable validated CPSSim model objects.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: nlohmann/json is confined to this translation unit. Unknown fields,
 *        wrong types, unsupported versions, and invalid model values are
 *        rejected rather than silently ignored.
 ***/

#include "cpssim/config/json_config.hpp"

#include "cpssim/model/identifiers.hpp"
#include "cpssim/model/specifications.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cpssim {

namespace {

using Json = nlohmann::json;

/***
 * Requires a JSON value to be an object in the named context.
 * Throws std::invalid_argument when the type is incorrect.
 ***/
void require_object(const Json& value, std::string_view context) {
    if (!value.is_object()) {
        throw std::invalid_argument{std::string{context} + " must be a JSON object"};
    }
}

/***
 * Verifies that an object contains no fields outside the supplied allowlist.
 * The context text identifies the offending schema location in errors.
 ***/
void require_only_fields(const Json& object, std::initializer_list<std::string_view> allowed_fields,
                         std::string_view context) {
    require_object(object, context);

    for (auto field = object.begin(); field != object.end(); ++field) {
        auto allowed = false;
        for (const auto allowed_field : allowed_fields) {
            if (field.key() == allowed_field) {
                allowed = true;
                break;
            }
        }

        if (!allowed) {
            throw std::invalid_argument{std::string{context} + " contains unknown field '" +
                                        field.key() + "'"};
        }
    }
}

/***
 * Finds and returns one required object field.
 * Throws std::invalid_argument when the named field is absent.
 ***/
const Json& require_field(const Json& object, std::string_view name, std::string_view context) {
    const auto field = object.find(name);
    if (field == object.end()) {
        throw std::invalid_argument{std::string{context} + " is missing field '" +
                                    std::string{name} + "'"};
    }
    return *field;
}

/***
 * Reads a JSON integer as a nonnegative 64-bit unsigned value.
 * Rejects negative values and non-integer JSON types.
 ***/
std::uint64_t read_unsigned(const Json& value, std::string_view context) {
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }
    if (value.is_number_integer()) {
        const auto signed_value = value.get<std::int64_t>();
        if (signed_value >= 0) {
            return static_cast<std::uint64_t>(signed_value);
        }
    }

    throw std::invalid_argument{std::string{context} + " must be a nonnegative integer"};
}

/***
 * Reads a JSON integer into the signed Tick domain without overflow.
 * Rejects values that cannot be represented by Tick.
 ***/
Tick read_tick(const Json& value, std::string_view context) {
    if (value.is_number_integer()) {
        return value.get<Tick>();
    }
    if (value.is_number_unsigned()) {
        const auto unsigned_value = value.get<std::uint64_t>();
        const auto maximum_tick = static_cast<std::uint64_t>(std::numeric_limits<Tick>::max());
        if (unsigned_value <= maximum_tick) {
            return static_cast<Tick>(unsigned_value);
        }
    }

    throw std::invalid_argument{std::string{context} + " must fit in a signed integer tick"};
}

/***
 * Reads task priority through the checked Tick parser and narrows it only when
 * the result fits in the configured Priority representation.
 ***/
Priority read_priority(const Json& value) {
    const auto priority = read_tick(value, "task priority");
    if (priority < std::numeric_limits<Priority>::min() ||
        priority > std::numeric_limits<Priority>::max()) {
        throw std::invalid_argument{"task priority is outside its supported range"};
    }
    return static_cast<Priority>(priority);
}

/***
 * Reads and returns a JSON string for the named schema context.
 * Throws std::invalid_argument for non-string values.
 ***/
std::string read_string(const Json& value, std::string_view context) {
    if (!value.is_string()) {
        throw std::invalid_argument{std::string{context} + " must be a string"};
    }
    return value.get<std::string>();
}

/*** Parses the explicit preemption behavior introduced by schema version 3. ***/
PreemptionMode parse_preemption_mode(const Json& value) {
    const auto mode = read_string(value, "scheduling.preemption");
    if (mode == "preemptive") {
        return PreemptionMode::Preemptive;
    }
    if (mode == "non_preemptive") {
        return PreemptionMode::NonPreemptive;
    }
    throw std::invalid_argument{"scheduling.preemption must be 'preemptive' or 'non_preemptive'"};
}

/*** Parses the version-3 experiment-wide scheduling assumptions. ***/
SchedulingSpec parse_scheduling(const Json& document) {
    const auto& scheduling = require_field(document, "scheduling", "experiment configuration");
    require_only_fields(scheduling, {"preemption"}, "scheduling");
    return SchedulingSpec{.preemption_mode = parse_preemption_mode(
                              require_field(scheduling, "preemption", "scheduling"))};
}

/***
 * Parses the version-1 deterministic execution-time object.
 * Returns its tick demand and rejects unsupported execution-time kinds.
 ***/
Tick parse_execution_time(const Json& value) {
    require_only_fields(value, {"kind", "ticks"}, "execution_time");

    const auto kind =
        read_string(require_field(value, "kind", "execution_time"), "execution_time.kind");
    if (kind != "deterministic") {
        throw std::invalid_argument{"execution_time.kind must be 'deterministic'"};
    }

    return read_tick(require_field(value, "ticks", "execution_time"), "execution_time.ticks");
}

/***
 * Parses one resource object and constructs its validated ResourceSpec.
 ***/
ResourceSpec parse_resource(const Json& value) {
    require_only_fields(value, {"id", "name"}, "resource");

    return ResourceSpec{
        ResourceId{read_unsigned(require_field(value, "id", "resource"), "resource.id")},
        read_string(require_field(value, "name", "resource"), "resource.name")};
}

/***
 * Parses one version-2 periodic task without choosing its runtime resource.
 ***/
TaskSpec parse_task_v2(const Json& value) {
    require_only_fields(
        value, {"id", "name", "period_ticks", "deadline_ticks", "offset_ticks", "priority"},
        "task");

    return TaskSpec{
        TaskId{read_unsigned(require_field(value, "id", "task"), "task.id")},
        read_string(require_field(value, "name", "task"), "task.name"),
        PeriodicTimingSpec{
            .period = read_tick(require_field(value, "period_ticks", "task"), "task.period_ticks"),
            .deadline =
                read_tick(require_field(value, "deadline_ticks", "task"), "task.deadline_ticks"),
            .offset = read_tick(require_field(value, "offset_ticks", "task"), "task.offset_ticks")},
        read_priority(require_field(value, "priority", "task"))};
}

/***
 * Parses one version-2 task-resource profile. This relation says that the
 * resource is accessible and gives the task's execution demand there.
 ***/
TaskResourceProfile parse_task_resource_profile(const Json& value) {
    require_only_fields(value, {"task_id", "resource_id", "execution_time"},
                        "task_resource_profile");

    return TaskResourceProfile{
        .task_id = TaskId{read_unsigned(require_field(value, "task_id", "task_resource_profile"),
                                        "task_resource_profile.task_id")},
        .resource_id =
            ResourceId{read_unsigned(require_field(value, "resource_id", "task_resource_profile"),
                                     "task_resource_profile.resource_id")},
        .execution_time =
            parse_execution_time(require_field(value, "execution_time", "task_resource_profile"))};
}

/*** Parses one schema-v4 completion-triggered fixed-delay message route. ***/
MessageRouteSpec parse_message_route(const Json& value) {
    require_only_fields(
        value, {"source_task_id", "destination_task_id", "send_offset_ticks", "delay_ticks"},
        "message_route");

    return MessageRouteSpec{
        .source_task_id =
            TaskId{read_unsigned(require_field(value, "source_task_id", "message_route"),
                                 "message_route.source_task_id")},
        .destination_task_id =
            TaskId{read_unsigned(require_field(value, "destination_task_id", "message_route"),
                                 "message_route.destination_task_id")},
        .send_offset = read_tick(require_field(value, "send_offset_ticks", "message_route"),
                                 "message_route.send_offset_ticks"),
        .delay = read_tick(require_field(value, "delay_ticks", "message_route"),
                           "message_route.delay_ticks")};
}

/***
 * Parses one legacy version-1 task. Its fixed mapping is translated into one
 * task-resource profile so old experiment files keep their original meaning.
 ***/
void parse_task_v1(const Json& value, std::vector<TaskSpec>& tasks,
                   std::vector<TaskResourceProfile>& profiles) {
    require_only_fields(value,
                        {"id", "name", "resource_id", "period_ticks", "deadline_ticks",
                         "offset_ticks", "priority", "execution_time"},
                        "task");

    const auto task_id = TaskId{read_unsigned(require_field(value, "id", "task"), "task.id")};
    const auto resource_id =
        ResourceId{read_unsigned(require_field(value, "resource_id", "task"), "task.resource_id")};
    tasks.emplace_back(
        task_id, read_string(require_field(value, "name", "task"), "task.name"),
        PeriodicTimingSpec{
            .period = read_tick(require_field(value, "period_ticks", "task"), "task.period_ticks"),
            .deadline =
                read_tick(require_field(value, "deadline_ticks", "task"), "task.deadline_ticks"),
            .offset = read_tick(require_field(value, "offset_ticks", "task"), "task.offset_ticks")},
        read_priority(require_field(value, "priority", "task")));
    profiles.push_back(TaskResourceProfile{
        .task_id = task_id,
        .resource_id = resource_id,
        .execution_time = parse_execution_time(require_field(value, "execution_time", "task"))});
}

/*** Reads the resource array shared by all supported schema versions. ***/
std::vector<ResourceSpec> parse_resources(const Json& document) {
    const auto& values = require_field(document, "resources", "experiment configuration");
    if (!values.is_array()) {
        throw std::invalid_argument{"resources must be a JSON array"};
    }

    std::vector<ResourceSpec> resources;
    resources.reserve(values.size());
    for (const auto& resource : values) {
        resources.push_back(parse_resource(resource));
    }
    return resources;
}

/*** Parses and translates the legacy fixed-resource version-1 schema. ***/
ExperimentConfig parse_document_v1(const Json& document) {
    require_only_fields(document, {"schema_version", "tick_period_ns", "resources", "tasks"},
                        "experiment configuration");

    const auto& task_values = require_field(document, "tasks", "experiment configuration");
    if (!task_values.is_array()) {
        throw std::invalid_argument{"tasks must be a JSON array"};
    }

    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    tasks.reserve(task_values.size());
    profiles.reserve(task_values.size());
    for (const auto& task : task_values) {
        parse_task_v1(task, tasks, profiles);
    }

    const auto tick_period_ns = read_tick(
        require_field(document, "tick_period_ns", "experiment configuration"), "tick_period_ns");
    return ExperimentConfig{std::chrono::nanoseconds{tick_period_ns},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            parse_resources(document), std::move(tasks), std::move(profiles)};
}

/*** Parses version 2 with separate tasks and many-to-many resource profiles. ***/
ExperimentConfig parse_document_v2(const Json& document) {
    require_only_fields(
        document,
        {"schema_version", "tick_period_ns", "resources", "tasks", "task_resource_profiles"},
        "experiment configuration");

    const auto& task_values = require_field(document, "tasks", "experiment configuration");
    const auto& profile_values =
        require_field(document, "task_resource_profiles", "experiment configuration");
    if (!task_values.is_array()) {
        throw std::invalid_argument{"tasks must be a JSON array"};
    }
    if (!profile_values.is_array()) {
        throw std::invalid_argument{"task_resource_profiles must be a JSON array"};
    }

    std::vector<TaskSpec> tasks;
    tasks.reserve(task_values.size());
    for (const auto& task : task_values) {
        tasks.push_back(parse_task_v2(task));
    }

    std::vector<TaskResourceProfile> profiles;
    profiles.reserve(profile_values.size());
    for (const auto& profile : profile_values) {
        profiles.push_back(parse_task_resource_profile(profile));
    }

    const auto tick_period_ns = read_tick(
        require_field(document, "tick_period_ns", "experiment configuration"), "tick_period_ns");
    return ExperimentConfig{std::chrono::nanoseconds{tick_period_ns},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            parse_resources(document), std::move(tasks), std::move(profiles)};
}

/*** Parses version 3 with explicit experiment-wide scheduling assumptions. ***/
ExperimentConfig parse_document_v3(const Json& document) {
    require_only_fields(document,
                        {"schema_version", "tick_period_ns", "scheduling", "resources", "tasks",
                         "task_resource_profiles"},
                        "experiment configuration");

    const auto& task_values = require_field(document, "tasks", "experiment configuration");
    const auto& profile_values =
        require_field(document, "task_resource_profiles", "experiment configuration");
    if (!task_values.is_array()) {
        throw std::invalid_argument{"tasks must be a JSON array"};
    }
    if (!profile_values.is_array()) {
        throw std::invalid_argument{"task_resource_profiles must be a JSON array"};
    }

    std::vector<TaskSpec> tasks;
    tasks.reserve(task_values.size());
    for (const auto& task : task_values) {
        tasks.push_back(parse_task_v2(task));
    }

    std::vector<TaskResourceProfile> profiles;
    profiles.reserve(profile_values.size());
    for (const auto& profile : profile_values) {
        profiles.push_back(parse_task_resource_profile(profile));
    }

    const auto tick_period_ns = read_tick(
        require_field(document, "tick_period_ns", "experiment configuration"), "tick_period_ns");
    return ExperimentConfig{std::chrono::nanoseconds{tick_period_ns}, parse_scheduling(document),
                            parse_resources(document), std::move(tasks), std::move(profiles)};
}

/*** Parses version 4 with completion-triggered fixed-delay message routes. ***/
ExperimentConfig parse_document_v4(const Json& document) {
    require_only_fields(document,
                        {"schema_version", "tick_period_ns", "scheduling", "resources", "tasks",
                         "task_resource_profiles", "message_routes"},
                        "experiment configuration");

    const auto& task_values = require_field(document, "tasks", "experiment configuration");
    const auto& profile_values =
        require_field(document, "task_resource_profiles", "experiment configuration");
    const auto& route_values =
        require_field(document, "message_routes", "experiment configuration");
    if (!task_values.is_array()) {
        throw std::invalid_argument{"tasks must be a JSON array"};
    }
    if (!profile_values.is_array()) {
        throw std::invalid_argument{"task_resource_profiles must be a JSON array"};
    }
    if (!route_values.is_array()) {
        throw std::invalid_argument{"message_routes must be a JSON array"};
    }

    std::vector<TaskSpec> tasks;
    tasks.reserve(task_values.size());
    for (const auto& task : task_values) {
        tasks.push_back(parse_task_v2(task));
    }

    std::vector<TaskResourceProfile> profiles;
    profiles.reserve(profile_values.size());
    for (const auto& profile : profile_values) {
        profiles.push_back(parse_task_resource_profile(profile));
    }

    std::vector<MessageRouteSpec> routes;
    routes.reserve(route_values.size());
    for (const auto& route : route_values) {
        routes.push_back(parse_message_route(route));
    }

    const auto tick_period_ns = read_tick(
        require_field(document, "tick_period_ns", "experiment configuration"), "tick_period_ns");
    return ExperimentConfig{std::chrono::nanoseconds{tick_period_ns},
                            parse_scheduling(document),
                            parse_resources(document),
                            std::move(tasks),
                            std::move(profiles),
                            std::move(routes)};
}

/*** Selects the parser for a supported top-level schema version. ***/
ExperimentConfig parse_document(const Json& document) {
    require_object(document, "experiment configuration");

    const auto schema_version = read_unsigned(
        require_field(document, "schema_version", "experiment configuration"), "schema_version");
    if (schema_version == 1) {
        return parse_document_v1(document);
    }
    if (schema_version == 2) {
        return parse_document_v2(document);
    }
    if (schema_version == 3) {
        return parse_document_v3(document);
    }
    if (schema_version == 4) {
        return parse_document_v4(document);
    }
    throw std::invalid_argument{"unsupported experiment schema version"};
}

} // namespace

/***
 * Converts JSON text into a document, translates parser failures to the
 * public invalid-argument error category, and validates the full schema.
 ***/
ExperimentConfig parse_experiment_config(std::string_view json_text) {
    Json document;
    try {
        document = Json::parse(json_text);
    } catch (const Json::exception& error) {
        throw std::invalid_argument{std::string{"invalid experiment JSON: "} + error.what()};
    }

    return parse_document(document);
}

/*** Serializes validated model values using the complete version-4 schema. ***/
std::string serialize_experiment_config_json(const ExperimentConfig& config) {
    const auto preemption = [&config] {
        switch (config.scheduling().preemption_mode) {
        case PreemptionMode::Preemptive:
            return "preemptive";
        case PreemptionMode::NonPreemptive:
            return "non_preemptive";
        }
        throw std::logic_error{"unknown preemption mode"};
    }();

    Json resources = Json::array();
    for (const auto& resource : config.resources()) {
        resources.push_back(Json{{"id", resource.id().value()}, {"name", resource.name()}});
    }

    Json tasks = Json::array();
    for (const auto& task : config.tasks()) {
        tasks.push_back(Json{{"deadline_ticks", task.deadline()},
                             {"id", task.id().value()},
                             {"name", task.name()},
                             {"offset_ticks", task.offset()},
                             {"period_ticks", task.period()},
                             {"priority", task.priority()}});
    }

    Json profiles = Json::array();
    for (const auto& profile : config.task_resource_profiles()) {
        profiles.push_back(Json{
            {"execution_time", Json{{"kind", "deterministic"}, {"ticks", profile.execution_time}}},
            {"resource_id", profile.resource_id.value()},
            {"task_id", profile.task_id.value()}});
    }

    Json routes = Json::array();
    for (const auto& route : config.message_routes()) {
        routes.push_back(Json{{"delay_ticks", route.delay},
                              {"destination_task_id", route.destination_task_id.value()},
                              {"send_offset_ticks", route.send_offset},
                              {"source_task_id", route.source_task_id.value()}});
    }

    const Json document{{"message_routes", std::move(routes)},
                        {"resources", std::move(resources)},
                        {"scheduling", Json{{"preemption", preemption}}},
                        {"schema_version", 4},
                        {"task_resource_profiles", std::move(profiles)},
                        {"tasks", std::move(tasks)},
                        {"tick_period_ns", config.tick_period().count()}};
    return document.dump(2) + '\n';
}

/***
 * Opens and reads the complete file, reports I/O failures with its path, and
 * parses the resulting text through the same public validation boundary.
 ***/
ExperimentConfig load_experiment_config(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot open experiment configuration: " + path.string()};
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad()) {
        throw std::runtime_error{"cannot read experiment configuration: " + path.string()};
    }

    return parse_experiment_config(contents.str());
}

void save_experiment_config(const std::filesystem::path& path, const ExperimentConfig& config) {
    const auto contents = serialize_experiment_config_json(config);
    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"cannot open experiment configuration for writing: " +
                                 path.string()};
    }
    output << contents;
    if (!output) {
        throw std::runtime_error{"cannot write experiment configuration: " + path.string()};
    }
}

} // namespace cpssim
