/***
 * File: tests/kernel/multi_resource_simulation_test.cpp
 * Purpose: Verify T10 independent-resource execution, deterministic same-tick
 *          ordering, configured mapping, validation, and repeatable traces.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/kernel/simulation_engine.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/policy/resource_allocator.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using cpssim::ConfiguredResourceAllocator;
using cpssim::Event;
using cpssim::EventType;
using cpssim::ExperimentConfig;
using cpssim::FixedPriorityPolicy;
using cpssim::PeriodicTimingSpec;
using cpssim::Priority;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::serialize_event_json_line;
using cpssim::SimulationEngine;
using cpssim::TaskAssignment;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;
using cpssim::Tick;

/*** Names one task and its one accessible resource for compact test setup. ***/
struct MappedTaskFixture {
    std::uint64_t task_id;
    std::uint64_t resource_id;
    Tick offset;
    Tick execution;
    Priority priority;
};

/*** Names the observable fields used for exact multi-resource trace ordering. ***/
struct ExpectedEvent {
    Tick tick;
    EventType type;
    TaskId task_id;
    ResourceId resource_id;
};

/*** Builds a test configuration with the supplied resource declaration order. ***/
ExperimentConfig make_config_with_resources(const std::vector<MappedTaskFixture>& fixtures,
                                            std::vector<ResourceSpec> resources) {
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    for (const auto& fixture : fixtures) {
        tasks.emplace_back(
            TaskId{fixture.task_id}, "task_" + std::to_string(fixture.task_id),
            PeriodicTimingSpec{.period = 100, .deadline = 20, .offset = fixture.offset},
            fixture.priority);
        profiles.push_back({.task_id = TaskId{fixture.task_id},
                            .resource_id = ResourceId{fixture.resource_id},
                            .execution_time = fixture.execution});
    }
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
        std::move(resources), std::move(tasks), std::move(profiles)};
}

/*** Builds two resources in reverse ID order to exercise canonical sorting. ***/
ExperimentConfig make_config(const std::vector<MappedTaskFixture>& fixtures) {
    return make_config_with_resources(
        fixtures, {ResourceSpec{ResourceId{2}, "cloud"}, ResourceSpec{ResourceId{1}, "local"}});
}

/*** Returns one explicit task mapping matching the compact fixtures. ***/
std::vector<TaskAssignment> make_assignments(const std::vector<MappedTaskFixture>& fixtures) {
    std::vector<TaskAssignment> assignments;
    assignments.reserve(fixtures.size());
    for (const auto& fixture : fixtures) {
        assignments.push_back(
            {.task_id = TaskId{fixture.task_id}, .resource_id = ResourceId{fixture.resource_id}});
    }
    return assignments;
}

/*** Compares processed trace fields without depending on unrelated event data. ***/
bool trace_matches(const std::vector<Event>& trace, const std::vector<ExpectedEvent>& expected) {
    if (trace.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < trace.size(); ++index) {
        const auto& entities = trace[index].entities();
        if (!entities.task_id.has_value() || !entities.resource_id.has_value()) {
            return false;
        }
        if (trace[index].tick() != expected[index].tick ||
            trace[index].type() != expected[index].type ||
            entities.task_id.value() != expected[index].task_id ||
            entities.resource_id.value() != expected[index].resource_id) {
            return false;
        }
    }
    return true;
}

/*** Runs and serializes one multi-resource experiment for repeatability checks. ***/
std::string serialized_trace(const ExperimentConfig& config,
                             const std::vector<TaskAssignment>& assignments, Tick stop_tick) {
    const ConfiguredResourceAllocator allocator{assignments};
    FixedPriorityPolicy policy;
    SimulationEngine engine{config, stop_tick, allocator, policy};
    engine.run();

    std::string output;
    for (const auto& event : engine.trace()) {
        output += serialize_event_json_line(event);
    }
    return output;
}

/*** Verifies that local preemption does not alter simultaneous cloud execution. ***/
TEST_CASE("independent resources execute and preempt without cross-resource interference",
          "[kernel][engine][multi-resource]") {
    const std::vector<MappedTaskFixture> fixtures{
        {1, 1, 0, 5, 2}, {2, 1, 2, 1, 1}, {3, 2, 0, 4, 3}};
    const auto config = make_config(fixtures);
    const ConfiguredResourceAllocator allocator{make_assignments(fixtures)};
    FixedPriorityPolicy policy;
    SimulationEngine engine{config, 6, allocator, policy};
    engine.run();

    const std::vector<ExpectedEvent> expected{{0, EventType::JobRelease, TaskId{1}, ResourceId{1}},
                                              {0, EventType::JobRelease, TaskId{3}, ResourceId{2}},
                                              {0, EventType::JobStart, TaskId{1}, ResourceId{1}},
                                              {0, EventType::JobStart, TaskId{3}, ResourceId{2}},
                                              {2, EventType::JobRelease, TaskId{2}, ResourceId{1}},
                                              {2, EventType::JobPreempt, TaskId{1}, ResourceId{1}},
                                              {2, EventType::JobStart, TaskId{2}, ResourceId{1}},
                                              {3, EventType::JobFinish, TaskId{2}, ResourceId{1}},
                                              {3, EventType::JobResume, TaskId{1}, ResourceId{1}},
                                              {4, EventType::JobFinish, TaskId{3}, ResourceId{2}},
                                              {6, EventType::JobFinish, TaskId{1}, ResourceId{1}}};

    const auto& runtime_scheduler = engine.scheduler();
    const auto& local_low = runtime_scheduler.jobs()[0];
    const auto& cloud = runtime_scheduler.jobs()[1];
    const bool state_matches =
        trace_matches(engine.trace(), expected) && runtime_scheduler.resource_count() == 2 &&
        local_low.preemption_count() == 1 && local_low.finish_tick() == 6 &&
        cloud.preemption_count() == 0 && cloud.finish_tick() == 4 &&
        runtime_scheduler.ready_jobs(ResourceId{1}).empty() &&
        !runtime_scheduler.resource(ResourceId{1}).running_job().has_value() &&
        runtime_scheduler.ready_jobs(ResourceId{2}).empty() &&
        !runtime_scheduler.resource(ResourceId{2}).running_job().has_value() &&
        runtime_scheduler.resource(ResourceId{1}).busy_ticks_until(6) == 6 &&
        runtime_scheduler.resource(ResourceId{1}).idle_ticks_until(6) == 0 &&
        runtime_scheduler.resource(ResourceId{2}).busy_ticks_until(6) == 4 &&
        runtime_scheduler.resource(ResourceId{2}).idle_ticks_until(6) == 2;
    REQUIRE(state_matches);
    REQUIRE_THROWS_AS(runtime_scheduler.resource(ResourceId{99}), std::logic_error);
}

/*** Verifies ResourceId order for simultaneous scheduling and completions. ***/
TEST_CASE("simultaneous resource events use ascending resource identifier order",
          "[kernel][engine][multi-resource][ordering]") {
    const std::vector<MappedTaskFixture> fixtures{{1, 1, 0, 2, 1}, {2, 2, 0, 2, 1}};
    const auto config = make_config(fixtures);
    const ConfiguredResourceAllocator allocator{make_assignments(fixtures)};
    FixedPriorityPolicy policy;
    SimulationEngine engine{config, 2, allocator, policy};
    engine.run();

    const std::vector<ExpectedEvent> expected{{0, EventType::JobRelease, TaskId{1}, ResourceId{1}},
                                              {0, EventType::JobRelease, TaskId{2}, ResourceId{2}},
                                              {0, EventType::JobStart, TaskId{1}, ResourceId{1}},
                                              {0, EventType::JobStart, TaskId{2}, ResourceId{2}},
                                              {2, EventType::JobFinish, TaskId{1}, ResourceId{1}},
                                              {2, EventType::JobFinish, TaskId{2}, ResourceId{2}}};
    REQUIRE(trace_matches(engine.trace(), expected));
}

/*** Verifies repeatability for the new multi-resource scheduling path. ***/
TEST_CASE("multi-resource processed traces are byte repeatable",
          "[kernel][engine][multi-resource][trace]") {
    const std::vector<MappedTaskFixture> fixtures{
        {1, 1, 0, 5, 2}, {2, 1, 2, 1, 1}, {3, 2, 0, 4, 3}};
    const auto config = make_config(fixtures);
    const auto assignments = make_assignments(fixtures);
    const auto first = serialized_trace(config, assignments, 6);
    const auto second = serialized_trace(config, assignments, 6);
    const bool traces_match = first == second;
    REQUIRE(traces_match);
}

/*** Verifies that resource array layout is not part of trace semantics. ***/
TEST_CASE("resource declaration order does not change the canonical trace",
          "[kernel][engine][multi-resource][ordering][trace]") {
    const std::vector<MappedTaskFixture> fixtures{{1, 1, 0, 2, 1}, {2, 2, 0, 2, 1}};
    const auto reverse_order = make_config(fixtures);
    const auto forward_order = make_config_with_resources(
        fixtures, {ResourceSpec{ResourceId{1}, "local"}, ResourceSpec{ResourceId{2}, "cloud"}});
    const auto assignments = make_assignments(fixtures);

    const bool traces_match = serialized_trace(reverse_order, assignments, 2) ==
                              serialized_trace(forward_order, assignments, 2);
    REQUIRE(traces_match);
}

/*** Verifies rejection of inaccessible and unknown configured mappings. ***/
TEST_CASE("multi-resource engine validates configured task mappings",
          "[kernel][engine][multi-resource][allocator]") {
    const std::vector<MappedTaskFixture> fixtures{{1, 1, 0, 2, 1}};
    const auto config = make_config(fixtures);
    FixedPriorityPolicy policy;
    const ConfiguredResourceAllocator inaccessible{
        {{.task_id = TaskId{1}, .resource_id = ResourceId{2}}}};
    const ConfiguredResourceAllocator unknown{
        {{.task_id = TaskId{1}, .resource_id = ResourceId{99}}}};

    REQUIRE_THROWS_AS((SimulationEngine{config, 2, inaccessible, policy}), std::invalid_argument);
    REQUIRE_THROWS_AS((SimulationEngine{config, 2, unknown, policy}), std::invalid_argument);
}

} // namespace
