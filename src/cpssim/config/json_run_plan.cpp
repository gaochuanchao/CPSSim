/***
 * File: src/cpssim/config/json_run_plan.cpp
 * Purpose: Implement deterministic run-plan JSON and located diagnostics.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/config/json_run_plan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cpssim {
namespace {

using Json = nlohmann::json;

[[noreturn]] void fail_at(std::string path, std::string message) {
    throw std::invalid_argument{"run plan " + std::move(path) + ": " + std::move(message)};
}

void require_object(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        fail_at(path, "must be a JSON object");
    }
}

void require_only_fields(const Json& object, std::initializer_list<std::string_view> allowed_fields,
                         const std::string& path) {
    require_object(object, path);
    for (auto field = object.begin(); field != object.end(); ++field) {
        const auto allowed =
            std::any_of(allowed_fields.begin(), allowed_fields.end(),
                        [&field](std::string_view candidate) { return field.key() == candidate; });
        if (!allowed) {
            fail_at(path + '.' + field.key(), "unknown field");
        }
    }
}

const Json& require_field(const Json& object, std::string_view name, const std::string& path) {
    const auto found = object.find(name);
    if (found == object.end()) {
        fail_at(path + '.' + std::string{name}, "missing required field");
    }
    return *found;
}

std::uint64_t read_unsigned(const Json& value, const std::string& path) {
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }
    if (value.is_number_integer()) {
        const auto signed_value = value.get<std::int64_t>();
        if (signed_value >= 0) {
            return static_cast<std::uint64_t>(signed_value);
        }
    }
    fail_at(path, "must be a nonnegative integer");
}

Tick read_tick(const Json& value, const std::string& path) {
    if (value.is_number_unsigned()) {
        const auto unsigned_value = value.get<std::uint64_t>();
        if (unsigned_value <= static_cast<std::uint64_t>(std::numeric_limits<Tick>::max())) {
            return static_cast<Tick>(unsigned_value);
        }
        fail_at(path, "must fit in a signed integer tick");
    }
    if (value.is_number_integer()) {
        return value.get<Tick>();
    }
    fail_at(path, "must fit in a signed integer tick");
}

std::string read_string(const Json& value, const std::string& path) {
    if (!value.is_string()) {
        fail_at(path, "must be a string");
    }
    return value.get<std::string>();
}

std::string preemption_name(PreemptionMode mode) {
    switch (mode) {
    case PreemptionMode::Preemptive:
        return "preemptive";
    case PreemptionMode::NonPreemptive:
        return "non_preemptive";
    }
    throw std::logic_error{"unknown preemption mode"};
}

Json build_experiment_signature(const ExperimentConfig& config) {
    auto resources = config.resources();
    std::sort(
        resources.begin(), resources.end(),
        [](const ResourceSpec& left, const ResourceSpec& right) { return left.id() < right.id(); });
    Json resource_values = Json::array();
    for (const auto& resource : resources) {
        resource_values.push_back(Json{{"id", resource.id().value()}, {"name", resource.name()}});
    }

    auto tasks = config.tasks();
    std::sort(tasks.begin(), tasks.end(),
              [](const TaskSpec& left, const TaskSpec& right) { return left.id() < right.id(); });
    Json task_values = Json::array();
    for (const auto& task : tasks) {
        task_values.push_back(Json{{"deadline_ticks", task.deadline()},
                                   {"id", task.id().value()},
                                   {"name", task.name()},
                                   {"offset_ticks", task.offset()},
                                   {"period_ticks", task.period()},
                                   {"priority", task.priority()}});
    }

    auto profiles = config.task_resource_profiles();
    std::sort(profiles.begin(), profiles.end(), [](const auto& left, const auto& right) {
        if (left.task_id != right.task_id) {
            return left.task_id < right.task_id;
        }
        return left.resource_id < right.resource_id;
    });
    Json profile_values = Json::array();
    for (const auto& profile : profiles) {
        profile_values.push_back(Json{{"execution_time_ticks", profile.execution_time},
                                      {"resource_id", profile.resource_id.value()},
                                      {"task_id", profile.task_id.value()}});
    }

    auto routes = config.message_routes();
    std::sort(routes.begin(), routes.end(), [](const auto& left, const auto& right) {
        if (left.source_task_id != right.source_task_id) {
            return left.source_task_id < right.source_task_id;
        }
        return left.destination_task_id < right.destination_task_id;
    });
    Json route_values = Json::array();
    for (const auto& route : routes) {
        route_values.push_back(Json{{"delay_ticks", route.delay},
                                    {"destination_task_id", route.destination_task_id.value()},
                                    {"send_offset_ticks", route.send_offset},
                                    {"source_task_id", route.source_task_id.value()}});
    }

    return Json{{"message_routes", std::move(route_values)},
                {"preemption", preemption_name(config.scheduling().preemption_mode)},
                {"resources", std::move(resource_values)},
                {"task_resource_profiles", std::move(profile_values)},
                {"tasks", std::move(task_values)},
                {"tick_period_ns", config.tick_period().count()}};
}

void compare_signature(const Json& actual, const Json& expected, const std::string& path) {
    if (expected.is_object()) {
        require_object(actual, path);
        for (auto field = actual.begin(); field != actual.end(); ++field) {
            if (!expected.contains(field.key())) {
                fail_at(path + '.' + field.key(), "unknown experiment-signature field");
            }
        }
        for (auto field = expected.begin(); field != expected.end(); ++field) {
            const auto actual_field = actual.find(field.key());
            const auto field_path = path + '.' + field.key();
            if (actual_field == actual.end()) {
                fail_at(field_path, "missing experiment-signature field");
            }
            compare_signature(*actual_field, field.value(), field_path);
        }
        return;
    }
    if (expected.is_array()) {
        if (!actual.is_array()) {
            fail_at(path, "must be an array");
        }
        if (actual.size() != expected.size()) {
            fail_at(path, "experiment mismatch: expected " + std::to_string(expected.size()) +
                              " item(s), found " + std::to_string(actual.size()));
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            compare_signature(actual[index], expected[index],
                              path + '[' + std::to_string(index) + ']');
        }
        return;
    }
    if (actual != expected) {
        fail_at(path,
                "experiment mismatch: expected " + expected.dump() + ", found " + actual.dump());
    }
}

SchedulingPolicyKind read_policy(const Json& value) {
    constexpr auto path = "$.scheduling_policy";
    require_only_fields(value, {"kind"}, path);
    const auto kind = read_string(require_field(value, "kind", path), "$.scheduling_policy.kind");
    if (kind != "fixed_priority") {
        fail_at("$.scheduling_policy.kind", "unsupported policy '" + kind + "'");
    }
    return SchedulingPolicyKind::FixedPriority;
}

std::vector<TaskAssignment> read_assignments(const Json& value) {
    constexpr auto path = "$.task_assignments";
    if (!value.is_array()) {
        fail_at(path, "must be an array");
    }
    std::vector<TaskAssignment> assignments;
    assignments.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto item_path = std::string{path} + '[' + std::to_string(index) + ']';
        const auto& item = value[index];
        require_only_fields(item, {"task_id", "resource_id"}, item_path);
        const auto task_id =
            read_unsigned(require_field(item, "task_id", item_path), item_path + ".task_id");
        const auto resource_id = read_unsigned(require_field(item, "resource_id", item_path),
                                               item_path + ".resource_id");
        assignments.push_back({.task_id = TaskId{task_id}, .resource_id = ResourceId{resource_id}});
    }
    return assignments;
}

std::size_t assignment_index(const RunPlanRequest& request, TaskId task_id,
                             std::optional<ResourceId> resource_id = std::nullopt,
                             std::size_t occurrence = 0) {
    std::size_t matched = 0;
    for (std::size_t index = 0; index < request.assignments.size(); ++index) {
        const auto& assignment = request.assignments[index];
        if (assignment.task_id == task_id &&
            (!resource_id.has_value() || assignment.resource_id == *resource_id)) {
            if (matched == occurrence) {
                return index;
            }
            ++matched;
        }
    }
    return 0;
}

TaskId require_task_id(const RunPlanDiagnostic& diagnostic) {
    if (!diagnostic.task_id.has_value()) {
        fail_at("$", "run-plan validation diagnostic omitted its task identifier");
    }
    return diagnostic.task_id.value();
}

ResourceId require_resource_id(const RunPlanDiagnostic& diagnostic) {
    if (!diagnostic.resource_id.has_value()) {
        fail_at("$", "run-plan validation diagnostic omitted its resource identifier");
    }
    return diagnostic.resource_id.value();
}

[[noreturn]] void fail_build(const RunPlanDiagnostic& diagnostic, const RunPlanRequest& request) {
    switch (diagnostic.code) {
    case RunPlanDiagnosticCode::InvalidStopTick:
        fail_at("$.stop_tick", diagnostic.message);
    case RunPlanDiagnosticCode::UnsupportedPolicy:
        fail_at("$.scheduling_policy.kind", diagnostic.message);
    case RunPlanDiagnosticCode::MissingTaskAssignment: {
        const auto task_id = require_task_id(diagnostic);
        fail_at("$.task_assignments",
                diagnostic.message + " for task T" + std::to_string(task_id.value()));
    }
    case RunPlanDiagnosticCode::DuplicateTaskAssignment: {
        const auto task_id = require_task_id(diagnostic);
        const auto index = assignment_index(request, task_id, std::nullopt, 1);
        fail_at("$.task_assignments[" + std::to_string(index) + "].task_id",
                diagnostic.message + " for task T" + std::to_string(task_id.value()));
    }
    case RunPlanDiagnosticCode::UnknownTask: {
        const auto task_id = require_task_id(diagnostic);
        const auto index = assignment_index(request, task_id, diagnostic.resource_id);
        fail_at("$.task_assignments[" + std::to_string(index) + "].task_id",
                diagnostic.message + " T" + std::to_string(task_id.value()));
    }
    case RunPlanDiagnosticCode::UnknownResource:
    case RunPlanDiagnosticCode::InaccessibleResource: {
        const auto task_id = require_task_id(diagnostic);
        const auto resource_id = require_resource_id(diagnostic);
        const auto index = assignment_index(request, task_id, resource_id);
        fail_at("$.task_assignments[" + std::to_string(index) + "].resource_id",
                diagnostic.message + " R" + std::to_string(resource_id.value()) + " for task T" +
                    std::to_string(task_id.value()));
    }
    case RunPlanDiagnosticCode::RunConstructionFailed:
        fail_at("$", diagnostic.message);
    }
    fail_at("$", "unknown validation failure");
}

RunPlan require_validated_plan(const RunPlanBuildResult& result, const RunPlanRequest& request) {
    if (!result.plan.has_value()) {
        if (result.diagnostics.empty()) {
            fail_at("$", "run-plan validation failed without a diagnostic");
        }
        fail_build(result.diagnostics.front(), request);
    }
    return result.plan.value();
}

RunPlan revalidate_plan(const ExperimentConfig& config, const RunPlan& plan) {
    const RunPlanRequest request{.stop_tick = plan.stop_tick(),
                                 .policy_kind = plan.policy_kind(),
                                 .assignments = plan.assignments()};
    const auto result = build_run_plan(config, request);
    return require_validated_plan(result, request);
}

} // namespace

std::string serialize_run_plan_json(const ExperimentConfig& config, const RunPlan& plan) {
    const auto validated = revalidate_plan(config, plan);
    auto assignments = validated.assignments();
    std::sort(assignments.begin(), assignments.end(),
              [](const TaskAssignment& left, const TaskAssignment& right) {
                  return left.task_id < right.task_id;
              });
    Json assignment_values = Json::array();
    for (const auto& assignment : assignments) {
        assignment_values.push_back(Json{{"resource_id", assignment.resource_id.value()},
                                         {"task_id", assignment.task_id.value()}});
    }

    const Json document{
        {"experiment_signature", build_experiment_signature(config)},
        {"schema_version", 1},
        {"scheduling_policy", Json{{"kind", "fixed_priority"}}},
        {"stop_tick", validated.stop_tick()},
        {"task_assignments", std::move(assignment_values)},
    };
    return document.dump(2) + '\n';
}

RunPlan parse_run_plan_json(std::string_view json_text, const ExperimentConfig& config) {
    Json document;
    try {
        document = Json::parse(json_text);
    } catch (const Json::parse_error& error) {
        fail_at("$ (byte " + std::to_string(error.byte) + ')', "malformed JSON");
    }

    require_only_fields(document,
                        {"schema_version", "experiment_signature", "stop_tick", "scheduling_policy",
                         "task_assignments"},
                        "$");
    const auto schema_version =
        read_unsigned(require_field(document, "schema_version", "$"), "$.schema_version");
    if (schema_version != 1) {
        fail_at("$.schema_version",
                "unsupported version " + std::to_string(schema_version) + "; expected 1");
    }

    compare_signature(require_field(document, "experiment_signature", "$"),
                      build_experiment_signature(config), "$.experiment_signature");

    const RunPlanRequest request{
        .stop_tick = read_tick(require_field(document, "stop_tick", "$"), "$.stop_tick"),
        .policy_kind = read_policy(require_field(document, "scheduling_policy", "$")),
        .assignments = read_assignments(require_field(document, "task_assignments", "$")),
    };
    const auto result = build_run_plan(config, request);
    return require_validated_plan(result, request);
}

RunPlan load_run_plan(const std::filesystem::path& path, const ExperimentConfig& config) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot open run-plan file '" + path.string() + "' for reading"};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        throw std::runtime_error{"failed while reading run-plan file '" + path.string() + "'"};
    }
    return parse_run_plan_json(buffer.str(), config);
}

void save_run_plan(const std::filesystem::path& path, const ExperimentConfig& config,
                   const RunPlan& plan) {
    const auto contents = serialize_run_plan_json(config, plan);
    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"cannot open run-plan file '" + path.string() + "' for writing"};
    }
    output << contents;
    if (!output) {
        throw std::runtime_error{"failed while writing run-plan file '" + path.string() + "'"};
    }
}

} // namespace cpssim
