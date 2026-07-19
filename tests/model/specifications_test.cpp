/***
 * File: tests/model/specifications_test.cpp
 * Purpose: Verify immutable resource/task values and complete experiment
 *          configuration validation introduced by T4.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/experiment_config.hpp"
#include "cpssim/model/specifications.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using cpssim::ExperimentConfig;
using cpssim::MessageRouteSpec;
using cpssim::PeriodicTimingSpec;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;

/***
 * Creates a valid periodic task with optional identity and name overrides.
 ***/
TaskSpec make_task(TaskId id = TaskId{1}, std::string name = "sensor") {
    return TaskSpec{id, std::move(name),
                    PeriodicTimingSpec{.period = 50, .deadline = 50, .offset = 0}, 1};
}

/***
 * Creates an ExperimentConfig from supplied collections using the Bosch
 * 100-microsecond tick period.
 ***/
ExperimentConfig
make_config(std::vector<ResourceSpec> resources, std::vector<TaskSpec> tasks,
            std::vector<TaskResourceProfile> profiles = {
                {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 6}}) {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
        std::move(resources), std::move(tasks), std::move(profiles)};
}

/*** Verifies that valid resource and task values remain available read-only. ***/
TEST_CASE("specifications retain validated configuration values", "[model][specification]") {
    const ResourceSpec resource{ResourceId{1}, "local_cpu"};
    const auto task = make_task();
    const auto resource_matches = resource.id() == ResourceId{1} && resource.name() == "local_cpu";
    const auto task_matches = task.id() == TaskId{1} && task.name() == "sensor" &&
                              task.period() == 50 && task.deadline() == 50 && task.offset() == 0 &&
                              task.priority() == 1;

    REQUIRE(resource_matches);
    REQUIRE(task_matches);
}

/*** Verifies every local ResourceSpec and TaskSpec construction constraint. ***/
TEST_CASE("local specification validation rejects invalid values", "[model][specification]") {
    REQUIRE_THROWS_AS((ResourceSpec{ResourceId{1}, ""}), std::invalid_argument);

    REQUIRE_THROWS_AS(
        (TaskSpec{TaskId{1}, "", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 1}),
        std::invalid_argument);
    REQUIRE_THROWS_AS((TaskSpec{TaskId{1}, "task",
                                PeriodicTimingSpec{.period = 0, .deadline = 10, .offset = 0}, 1}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((TaskSpec{TaskId{1}, "task",
                                PeriodicTimingSpec{.period = 10, .deadline = 0, .offset = 0}, 1}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((TaskSpec{TaskId{1}, "task",
                                PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = -1}, 1}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((TaskSpec{TaskId{1}, "task",
                                PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, -1}),
                      std::invalid_argument);
}

/*** Verifies collection uniqueness, required contents, and resource references. ***/
TEST_CASE("experiment configuration validates collections and references",
          "[model][configuration]") {
    const auto valid = make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()});
    const auto valid_matches =
        valid.tick_period() == std::chrono::nanoseconds{100'000} &&
        valid.scheduling().preemption_mode == cpssim::PreemptionMode::Preemptive &&
        valid.resources().size() == 1 && valid.tasks().size() == 1 &&
        valid.task_resource_profiles().size() == 1;

    REQUIRE(valid_matches);

    REQUIRE_THROWS_AS(
        (ExperimentConfig{
            std::chrono::nanoseconds{0},
            cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
            {ResourceSpec{ResourceId{1}, "local_cpu"}},
            {make_task()},
            {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 6}}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(make_config({}, {make_task()}), std::invalid_argument);
    REQUIRE_THROWS_AS(make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "first"}, ResourceSpec{ResourceId{1}, "second"}},
                    {make_task()}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "same"}, ResourceSpec{ResourceId{2}, "same"}},
                    {make_task()}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(make_config({ResourceSpec{ResourceId{1}, "local_cpu"}},
                                  {make_task(TaskId{1}, "first"), make_task(TaskId{1}, "second")}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make_config({ResourceSpec{ResourceId{1}, "local_cpu"}},
                                  {make_task(TaskId{1}, "same"), make_task(TaskId{2}, "same")}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()},
                    {{.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 6}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 6}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 0}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 51}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make_config({ResourceSpec{ResourceId{1}, "local_cpu"}}, {make_task()},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 6},
                     {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 7}}),
        std::invalid_argument);
}

/*** Verifies route retention and cross-record endpoint/timing validation. ***/
TEST_CASE("experiment configuration validates message routes", "[model][configuration][network]") {
    const std::vector<ResourceSpec> resources{ResourceSpec{ResourceId{1}, "cpu"}};
    const std::vector<TaskSpec> tasks{make_task(TaskId{1}, "producer"),
                                      make_task(TaskId{2}, "consumer")};
    const std::vector<TaskResourceProfile> profiles{
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 6},
        {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 6}};
    const auto make_with_routes = [&](std::vector<MessageRouteSpec> routes) {
        return ExperimentConfig{
            std::chrono::nanoseconds{100'000},
            cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
            resources,
            tasks,
            profiles,
            std::move(routes)};
    };

    const auto valid = make_with_routes({MessageRouteSpec{.source_task_id = TaskId{1},
                                                          .destination_task_id = TaskId{2},
                                                          .send_offset = 1,
                                                          .delay = 80}});
    const auto valid_route_count_matches = valid.message_routes().size() == 1;
    REQUIRE(valid_route_count_matches);

    REQUIRE_THROWS_AS(make_with_routes({MessageRouteSpec{.source_task_id = TaskId{3},
                                                         .destination_task_id = TaskId{2},
                                                         .send_offset = 1,
                                                         .delay = 80}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make_with_routes({MessageRouteSpec{.source_task_id = TaskId{1},
                                                         .destination_task_id = TaskId{2},
                                                         .send_offset = 0,
                                                         .delay = 80}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make_with_routes({MessageRouteSpec{.source_task_id = TaskId{1},
                                                         .destination_task_id = TaskId{2},
                                                         .send_offset = 1,
                                                         .delay = 0}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make_with_routes({MessageRouteSpec{.source_task_id = TaskId{1},
                                                         .destination_task_id = TaskId{2},
                                                         .send_offset = 1,
                                                         .delay = 80},
                                        MessageRouteSpec{.source_task_id = TaskId{1},
                                                         .destination_task_id = TaskId{2},
                                                         .send_offset = 2,
                                                         .delay = 90}}),
                      std::invalid_argument);
}

} // namespace
