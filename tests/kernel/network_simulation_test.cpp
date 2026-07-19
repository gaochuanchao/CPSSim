/***
 * File: tests/kernel/network_simulation_test.cpp
 * Purpose: Verify completion-triggered causal messages inside the complete
 *          engine cycle, including phase order, stale events, and repeatability.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/kernel/simulation_engine.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/policy/resource_allocator.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {

using cpssim::EventType;
using cpssim::ExperimentConfig;
using cpssim::FixedPriorityPolicy;
using cpssim::JobId;
using cpssim::MessageLifecycle;
using cpssim::MessageRouteSpec;
using cpssim::PeriodicTimingSpec;
using cpssim::PreemptionMode;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::SchedulingSpec;
using cpssim::serialize_event_json_line;
using cpssim::SimulationEngine;
using cpssim::SingleResourceAllocator;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;
using cpssim::Tick;

/*** Creates a producer and a destination whose release shares delivery tick 7. ***/
ExperimentConfig make_message_config() {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "producer",
                  PeriodicTimingSpec{.period = 100, .deadline = 100, .offset = 0}, 1},
         TaskSpec{TaskId{2}, "destination",
                  PeriodicTimingSpec{.period = 100, .deadline = 100, .offset = 7}, 1}},
        {TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 2},
         TaskResourceProfile{
             .task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 1}},
        {MessageRouteSpec{.source_task_id = TaskId{1},
                          .destination_task_id = TaskId{2},
                          .send_offset = 1,
                          .delay = 4}}};
}

/*** Serializes one complete causal-message trace for repeatability checking. ***/
std::string serialized_trace() {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_message_config(), 7, allocator, policy};
    engine.run();

    std::string result;
    for (const auto& event : engine.trace()) {
        result += serialize_event_json_line(event);
    }
    return result;
}

/*** Verifies finish/send/delivery causality and delivery-before-release order. ***/
TEST_CASE("simulation engine processes causal messages in deterministic phases",
          "[kernel][network][causality]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_message_config(), 7, allocator, policy};
    engine.run();

    const auto& trace = engine.trace();
    std::vector<std::pair<Tick, EventType>> shape;
    shape.reserve(trace.size());
    for (const auto& event : trace) {
        shape.emplace_back(event.tick(), event.type());
    }
    const std::vector<std::pair<Tick, EventType>> expected{
        {0, EventType::JobRelease},  {0, EventType::JobStart},        {2, EventType::JobFinish},
        {3, EventType::MessageSend}, {7, EventType::MessageDelivery}, {7, EventType::JobRelease},
        {7, EventType::JobStart}};
    const auto shape_matches = shape == expected;
    REQUIRE(shape_matches);

    const auto& message = engine.network().messages().front();
    const auto message_matches = message.source_job().task_id() == TaskId{1} &&
                                 message.source_job().job_id() == JobId{1} &&
                                 message.destination_task_id() == TaskId{2} &&
                                 message.lifecycle() == MessageLifecycle::Delivered;
    REQUIRE(message_matches);

    const auto& finish = trace[2];
    const auto& send = trace[3];
    const auto& delivery = trace[4];
    const auto causes_match =
        send.cause_sequence() == finish.sequence() && delivery.cause_sequence() == send.sequence();
    REQUIRE(causes_match);
}

/*** Verifies that an obsolete completion candidate cannot publish a message. ***/
TEST_CASE("stale completion events do not create duplicate messages", "[kernel][network][stale]") {
    const auto config = ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "producer",
                  PeriodicTimingSpec{.period = 100, .deadline = 20, .offset = 0}, 2},
         TaskSpec{TaskId{2}, "interrupt",
                  PeriodicTimingSpec{.period = 100, .deadline = 20, .offset = 2}, 1},
         TaskSpec{TaskId{3}, "destination",
                  PeriodicTimingSpec{.period = 100, .deadline = 20, .offset = 20}, 1}},
        {TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 6},
         TaskResourceProfile{
             .task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 2},
         TaskResourceProfile{
             .task_id = TaskId{3}, .resource_id = ResourceId{1}, .execution_time = 1}},
        {MessageRouteSpec{.source_task_id = TaskId{1},
                          .destination_task_id = TaskId{3},
                          .send_offset = 1,
                          .delay = 2}}};

    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{config, 12, allocator, policy};
    engine.run();

    const auto publication_matches = engine.network().messages().size() == 1 &&
                                     engine.network().messages().front().publish_tick() == 8;
    REQUIRE(publication_matches);
}

/*** Verifies byte-identical causal traces across fresh runs. ***/
TEST_CASE("causal message traces are byte repeatable", "[kernel][network][trace]") {
    const auto repeatable_bytes = serialized_trace() == serialized_trace();
    REQUIRE(repeatable_bytes);
}

} // namespace
