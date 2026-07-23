/***
 * File: tests/gui/presentation_model_test.cpp
 * Purpose: Verify complete, detached, and deterministically ordered G02
 *          experiment presentation records.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/presentation_model.hpp"
#include "cpssim/gui/simulation_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

using namespace cpssim;

/*** Builds equivalent multi-entity configurations in either declaration order. ***/
ExperimentConfig make_presentation_config(bool reverse_declarations) {
    std::vector<ResourceSpec> resources;
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    std::vector<MessageRouteSpec> routes;

    if (reverse_declarations) {
        resources.emplace_back(ResourceId{2}, "cloud");
        resources.emplace_back(ResourceId{1}, "local");
        tasks.emplace_back(TaskId{2}, "controller",
                           PeriodicTimingSpec{.period = 20, .deadline = 15, .offset = 2}, 1);
        tasks.emplace_back(TaskId{1}, "sensor",
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 0);
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{2}, .execution_time = 4});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 3});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 2});
        routes.push_back({.source_task_id = TaskId{2},
                          .destination_task_id = TaskId{1},
                          .send_offset = message_route_send_offset_ticks,
                          .delay = 5});
        routes.push_back({.source_task_id = TaskId{1},
                          .destination_task_id = TaskId{2},
                          .send_offset = message_route_send_offset_ticks,
                          .delay = 3});
    } else {
        resources.emplace_back(ResourceId{1}, "local");
        resources.emplace_back(ResourceId{2}, "cloud");
        tasks.emplace_back(TaskId{1}, "sensor",
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 0);
        tasks.emplace_back(TaskId{2}, "controller",
                           PeriodicTimingSpec{.period = 20, .deadline = 15, .offset = 2}, 1);
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 3});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{2}, .execution_time = 4});
        routes.push_back({.source_task_id = TaskId{1},
                          .destination_task_id = TaskId{2},
                          .send_offset = message_route_send_offset_ticks,
                          .delay = 3});
        routes.push_back({.source_task_id = TaskId{2},
                          .destination_task_id = TaskId{1},
                          .send_offset = message_route_send_offset_ticks,
                          .delay = 5});
    }

    return ExperimentConfig{std::chrono::microseconds{100},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            std::move(resources),
                            std::move(tasks),
                            std::move(profiles),
                            std::move(routes)};
}

/*** Verifies completeness, stable order, and exactly one applied assignment. ***/
TEST_CASE("GUI experiment presentation contains complete sorted configuration",
          "[gui][presentation]") {
    const auto config = make_presentation_config(true);
    const std::vector<TaskAssignment> assignments{
        {.task_id = TaskId{2}, .resource_id = ResourceId{2}},
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}}};

    const auto presentation = build_experiment_presentation(config, assignments);

    REQUIRE((presentation.tick_period == std::chrono::microseconds{100}));
    REQUIRE((presentation.preemption_mode == PreemptionMode::Preemptive));
    REQUIRE((presentation.resources.size() == 2));
    REQUIRE((presentation.resources[0].id == ResourceId{1}));
    REQUIRE((presentation.resources[0].name == "local"));
    REQUIRE((presentation.resources[1].id == ResourceId{2}));
    REQUIRE((presentation.tasks.size() == 2));
    REQUIRE((presentation.tasks[0].id == TaskId{1}));
    REQUIRE((presentation.tasks[0].period == 10));
    REQUIRE((presentation.tasks[0].deadline == 10));
    REQUIRE((presentation.tasks[0].offset == 0));
    REQUIRE((presentation.tasks[0].priority == 0));
    REQUIRE((presentation.tasks[1].id == TaskId{2}));
    REQUIRE((presentation.profiles.size() == 3));
    REQUIRE((presentation.profiles[0].task_id == TaskId{1}));
    REQUIRE((presentation.profiles[0].resource_id == ResourceId{1}));
    REQUIRE((presentation.profiles[0].execution_time == 2));
    REQUIRE((presentation.profiles[2].task_id == TaskId{2}));
    REQUIRE((presentation.profiles[2].resource_id == ResourceId{2}));
    REQUIRE((presentation.profiles[2].execution_time == 4));
    REQUIRE((presentation.routes.size() == 2));
    REQUIRE((presentation.routes[0].identity == GuiRouteIdentity{TaskId{1}, TaskId{2}}));
    REQUIRE((presentation.routes[0].send_offset == 1));
    REQUIRE((presentation.routes[0].delay == 3));
    REQUIRE((presentation.assignments.size() == 2));
    REQUIRE((presentation.assignments[0].task_id == TaskId{1}));
    REQUIRE((presentation.assignments[0].resource_id == ResourceId{1}));
    REQUIRE((presentation.assignments[1].task_id == TaskId{2}));
    REQUIRE((presentation.assignments[1].resource_id == ResourceId{2}));
}

/*** Proves declaration order cannot change the derived presentation record. ***/
TEST_CASE("GUI experiment presentation is independent of declaration order",
          "[gui][presentation][determinism]") {
    const std::vector<TaskAssignment> forward_assignments{
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}},
        {.task_id = TaskId{2}, .resource_id = ResourceId{2}}};
    const std::vector<TaskAssignment> reverse_assignments{
        {.task_id = TaskId{2}, .resource_id = ResourceId{2}},
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}}};

    REQUIRE((build_experiment_presentation(make_presentation_config(false), forward_assignments) ==
             build_experiment_presentation(make_presentation_config(true), reverse_assignments)));
}

/*** Verifies snapshot mutation cannot reach controller-owned presentation. ***/
TEST_CASE("GUI experiment presentation snapshot is detached", "[gui][presentation][snapshot]") {
    const auto config = make_presentation_config(false);
    const auto plan = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 20,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{2}, .resource_id = ResourceId{2}}}});
    REQUIRE(plan.valid());
    SimulationController controller{config, *plan.plan};

    auto detached = controller.snapshot();
    detached.experiment.tasks[0].name = "mutated display copy";
    detached.experiment.assignments.clear();

    const auto fresh = controller.snapshot();
    REQUIRE((fresh.experiment.tasks[0].name == "sensor"));
    REQUIRE((fresh.experiment.assignments.size() == 2));
}

/*** Rejects an ambiguous or incomplete applied plan in the public builder. ***/
TEST_CASE("GUI experiment presentation rejects invalid assignments",
          "[gui][presentation][invalid]") {
    const auto config = make_presentation_config(false);

    REQUIRE_THROWS_AS(build_experiment_presentation(
                          config, {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(build_experiment_presentation(
                          config, {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                   {.task_id = TaskId{1}, .resource_id = ResourceId{2}}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(build_experiment_presentation(
                          config, {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                   {.task_id = TaskId{2}, .resource_id = ResourceId{1}}}),
                      std::invalid_argument);
}

} // namespace
