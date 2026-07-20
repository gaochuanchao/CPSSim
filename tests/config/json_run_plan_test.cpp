/***
 * File: tests/config/json_run_plan_test.cpp
 * Purpose: Verify deterministic located run-plan JSON persistence.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/config/json_run_plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

using namespace cpssim;

ExperimentConfig make_persistence_config(bool reverse = false, std::string first_name = "first") {
    std::vector<ResourceSpec> resources;
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    if (reverse) {
        resources.emplace_back(ResourceId{2}, "cloud");
        resources.emplace_back(ResourceId{1}, "local");
        tasks.emplace_back(TaskId{2}, "second",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 1}, 2);
        tasks.emplace_back(TaskId{1}, std::move(first_name),
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 1);
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 3});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1});
    } else {
        resources.emplace_back(ResourceId{1}, "local");
        resources.emplace_back(ResourceId{2}, "cloud");
        tasks.emplace_back(TaskId{1}, std::move(first_name),
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 1);
        tasks.emplace_back(TaskId{2}, "second",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 1}, 2);
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 3});
    }
    return ExperimentConfig{std::chrono::nanoseconds{100'000},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            std::move(resources), std::move(tasks), std::move(profiles)};
}

RunPlan make_plan(const ExperimentConfig& config) {
    const auto result = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 300,
                       .assignments = {{.task_id = TaskId{2}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{1}, .resource_id = ResourceId{2}}}});
    if (!result.valid()) {
        throw std::logic_error{"test plan must be valid"};
    }
    return *result.plan;
}

std::string invalid_message(std::string_view text, const ExperimentConfig& config) {
    try {
        static_cast<void>(parse_run_plan_json(text, config));
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    throw std::logic_error{"test input unexpectedly parsed"};
}

TEST_CASE("run-plan JSON round trips with deterministic canonical ordering",
          "[config][run-plan][json]") {
    const auto forward = make_persistence_config();
    const auto reverse = make_persistence_config(true);
    const auto serialized = serialize_run_plan_json(forward, make_plan(forward));

    REQUIRE((serialized == serialize_run_plan_json(forward, make_plan(forward))));
    REQUIRE((serialized == serialize_run_plan_json(reverse, make_plan(reverse))));
    REQUIRE((serialized.back() == '\n'));
    REQUIRE((parse_run_plan_json(serialized, forward) == make_plan(forward)));
    REQUIRE((parse_run_plan_json(serialized, reverse) == make_plan(reverse)));
}

TEST_CASE("run-plan JSON reports precise malformed and schema locations",
          "[config][run-plan][json][invalid]") {
    const auto config = make_persistence_config();
    REQUIRE((invalid_message("{", config).find("$ (byte 2)") != std::string::npos));

    auto serialized = serialize_run_plan_json(config, make_plan(config));
    serialized.replace(serialized.find("\"schema_version\": 1"), 19, "\"schema_version\": 2");
    REQUIRE((invalid_message(serialized, config).find("$.schema_version") != std::string::npos));

    serialized = serialize_run_plan_json(config, make_plan(config));
    serialized.insert(serialized.find('{') + 1, "\n  \"unexpected\": true,");
    REQUIRE((invalid_message(serialized, config).find("$.unexpected") != std::string::npos));

    serialized = serialize_run_plan_json(config, make_plan(config));
    const auto stop_tick = serialized.find("\"stop_tick\": 300");
    serialized.replace(stop_tick, 16, "\"stop_tick\": 18446744073709551615");
    REQUIRE((invalid_message(serialized, config).find("$.stop_tick") != std::string::npos));
}

TEST_CASE("run-plan JSON reports the exact experiment mismatch location",
          "[config][run-plan][json][association]") {
    const auto config = make_persistence_config();
    const auto serialized = serialize_run_plan_json(config, make_plan(config));
    const auto renamed = make_persistence_config(false, "renamed");

    const auto message = invalid_message(serialized, renamed);
    REQUIRE((message.find("$.experiment_signature.tasks[0].name") != std::string::npos));
    REQUIRE((message.find("expected \"renamed\"") != std::string::npos));
}

TEST_CASE("run-plan JSON locates assignment validation failures",
          "[config][run-plan][json][assignment]") {
    const auto config = make_persistence_config();
    auto serialized = serialize_run_plan_json(config, make_plan(config));
    const auto resource =
        serialized.find("\"resource_id\": 2", serialized.find("\"task_assignments\""));
    serialized.replace(resource, 16, "\"resource_id\": 99");
    REQUIRE((invalid_message(serialized, config).find("$.task_assignments[0].resource_id") !=
             std::string::npos));

    serialized = serialize_run_plan_json(config, make_plan(config));
    const auto second_task =
        serialized.find("\"task_id\": 2", serialized.find("\"task_assignments\""));
    serialized.replace(second_task, 12, "\"task_id\": 1");
    REQUIRE((invalid_message(serialized, config).find("$.task_assignments[1].task_id") !=
             std::string::npos));

    serialized = serialize_run_plan_json(config, make_plan(config));
    const auto unknown_task =
        serialized.find("\"task_id\": 2", serialized.find("\"task_assignments\""));
    serialized.replace(unknown_task, 12, "\"task_id\": 99");
    REQUIRE((invalid_message(serialized, config).find("$.task_assignments[1].task_id") !=
             std::string::npos));

    serialized = serialize_run_plan_json(config, make_plan(config));
    const auto inaccessible_resource =
        serialized.find("\"resource_id\": 1", serialized.find("\"task_assignments\""));
    serialized.replace(inaccessible_resource, 16, "\"resource_id\": 2");
    REQUIRE((invalid_message(serialized, config).find("$.task_assignments[1].resource_id") !=
             std::string::npos));
}

TEST_CASE("run-plan files use the same validated persistence boundary",
          "[config][run-plan][json][file]") {
    const auto config = make_persistence_config();
    const auto path = std::filesystem::temp_directory_path() / "cpssim-run-plan-test.json";
    save_run_plan(path, config, make_plan(config));
    REQUIRE((load_run_plan(path, config) == make_plan(config)));
    std::filesystem::remove(path);
    REQUIRE_THROWS_AS(load_run_plan(path, config), std::runtime_error);
}

} // namespace
