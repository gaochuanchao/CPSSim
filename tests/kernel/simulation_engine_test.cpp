/***
 * File: tests/kernel/simulation_engine_test.cpp
 * Purpose: Verify the T9 one-resource event-driven scheduler, execution
 *          accounting, preemption, deadlines, overlap rejection, and traces.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/functional/mock_functional_model.hpp"
#include "cpssim/kernel/simulation_engine.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/policy/resource_allocator.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using cpssim::EventType;
using cpssim::ExperimentConfig;
using cpssim::FixedPriorityPolicy;
using cpssim::JobId;
using cpssim::JobIdentity;
using cpssim::JobLifecycle;
using cpssim::PeriodicTimingSpec;
using cpssim::PreemptionMode;
using cpssim::Priority;
using cpssim::ResourceAllocator;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::SchedulingPolicy;
using cpssim::serialize_event_json_line;
using cpssim::SimulationEngine;
using cpssim::SingleResourceAllocator;
using cpssim::TaskAssignment;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;
using cpssim::Tick;

/*** Names the immutable values needed for one synthetic periodic task. ***/
struct TaskFixture {
    std::uint64_t id;
    Tick period;
    Tick deadline;
    Tick offset;
    Tick execution;
    Priority priority;
};

/*** Test scheduler that selects the last Ready identity and never preempts. ***/
class LastReadyPolicy : public SchedulingPolicy {
  public:
    JobIdentity select(const cpssim::Resource&, const std::vector<JobIdentity>& ready_jobs,
                       const std::vector<cpssim::JobState>&) const override {
        return ready_jobs.back();
    }

    bool should_preempt(const cpssim::JobState&, const cpssim::JobState&) const override {
        return false;
    }
};

/*** Minimal policy that changes selection after receiving model observations. ***/
class ObservationAwarePolicy : public SchedulingPolicy {
  public:
    /*** Caches only whether the required mock signal has been observed. ***/
    void observe(const cpssim::FunctionalObservation& observation) override {
        ++observation_count_;
        for (const auto& signal : observation.real_signals) {
            if (signal.name == "mock_state" && signal.value == 0.0) {
                choose_last_ = true;
            }
        }
    }

    /*** Uses the observation-derived flag without changing simulator state. ***/
    JobIdentity select(const cpssim::Resource&, const std::vector<JobIdentity>& ready_jobs,
                       const std::vector<cpssim::JobState>&) const override {
        return choose_last_ ? ready_jobs.back() : ready_jobs.front();
    }

    // This test policy never replaces a Running job.
    bool should_preempt(const cpssim::JobState&, const cpssim::JobState&) const override {
        return false;
    }

    // Reports how many physical observations reached the policy.
    std::size_t observation_count() const { return observation_count_; }

  private:
    bool choose_last_{false};
    std::size_t observation_count_{0};
};

/*** Test allocator that deliberately omits all but one configured task. ***/
class IncompleteAllocator : public ResourceAllocator {
  public:
    std::vector<TaskAssignment> allocate(const ExperimentConfig&) const override {
        return {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}};
    }
};

/*** Test allocator that assigns one task twice and omits another task. ***/
class DuplicateAllocator : public ResourceAllocator {
  public:
    std::vector<TaskAssignment> allocate(const ExperimentConfig&) const override {
        return {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                {.task_id = TaskId{1}, .resource_id = ResourceId{1}}};
    }
};

/*** Creates a one-resource configuration from compact synthetic task values. ***/
ExperimentConfig make_config(const std::vector<TaskFixture>& fixtures,
                             PreemptionMode preemption_mode = PreemptionMode::Preemptive) {
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    for (const auto& fixture : fixtures) {
        tasks.emplace_back(TaskId{fixture.id}, "task_" + std::to_string(fixture.id),
                           PeriodicTimingSpec{.period = fixture.period,
                                              .deadline = fixture.deadline,
                                              .offset = fixture.offset},
                           fixture.priority);
        profiles.push_back({.task_id = TaskId{fixture.id},
                            .resource_id = ResourceId{1},
                            .execution_time = fixture.execution});
    }
    return ExperimentConfig{std::chrono::nanoseconds{100'000},
                            cpssim::SchedulingSpec{.preemption_mode = preemption_mode},
                            {ResourceSpec{ResourceId{1}, "cpu"}},
                            std::move(tasks),
                            std::move(profiles)};
}

/*** Extracts the tick/type pair used by concise scheduling-trace assertions. ***/
std::vector<std::pair<Tick, EventType>> trace_shape(const SimulationEngine& engine) {
    std::vector<std::pair<Tick, EventType>> result;
    for (const auto& event : engine.trace()) {
        result.emplace_back(event.tick(), event.type());
    }
    return result;
}

/*** Serializes a complete processed trace for byte-repeatability checks. ***/
std::string serialized_trace(const ExperimentConfig& config, Tick stop_tick) {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{config, stop_tick, allocator, policy};
    engine.run();
    std::string output;
    for (const auto& event : engine.trace()) {
        output += serialize_event_json_line(event);
    }
    return output;
}

/*** Verifies releases, execution completion, and inclusive stop-tick dispatch. ***/
TEST_CASE("one periodic task releases starts and completes event-driven jobs", "[kernel][engine]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_config({{1, 10, 10, 0, 3, 1}}), 10, allocator, policy};
    engine.run();

    const std::vector<std::pair<Tick, EventType>> expected{{0, EventType::JobRelease},
                                                           {0, EventType::JobStart},
                                                           {3, EventType::JobFinish},
                                                           {10, EventType::JobRelease},
                                                           {10, EventType::JobStart}};
    const auto& jobs = engine.scheduler().jobs();
    const auto state_matches =
        trace_shape(engine) == expected && jobs.size() == 2 && jobs[0].finish_tick() == 3 &&
        jobs[0].lifecycle() == JobLifecycle::Completed &&
        jobs[1].lifecycle() == JobLifecycle::Running && engine.current_tick() == 10;

    REQUIRE(state_matches);
    REQUIRE_THROWS_AS(engine.run(), std::logic_error);
}

/***
 * Verifies that runtime-generated releases at one tick use task semantics,
 * even when their successor events entered the queue at different times.
 ***/
TEST_CASE("same-tick releases are processed by priority then task ID",
          "[kernel][engine][release-ordering]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{
        make_config({{1, 10, 10, 0, 1, 2}, {2, 20, 20, 0, 1, 2}, {3, 20, 20, 0, 1, 1}}), 20,
        allocator, policy};
    engine.run();

    std::vector<TaskId> releases_at_twenty;
    for (const auto& event : engine.trace()) {
        if (event.tick() == 20 && event.type() == EventType::JobRelease) {
            const auto task_id = event.entities().task_id;
            if (!task_id.has_value()) {
                throw std::logic_error{"release regression event lacks a task ID"};
            }
            releases_at_twenty.push_back(task_id.value());
        }
    }

    const bool order_matches =
        releases_at_twenty == std::vector<TaskId>{TaskId{3}, TaskId{1}, TaskId{2}};
    REQUIRE(order_matches);
}

/*** Verifies exact charging, strict preemption, and later resumption. ***/
TEST_CASE("a higher-priority release preempts and the previous job resumes", "[kernel][engine]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_config({{1, 100, 20, 0, 6, 2}, {2, 100, 10, 2, 2, 1}}), 10,
                            allocator, policy};
    engine.run();

    const std::vector<std::pair<Tick, EventType>> expected{
        {0, EventType::JobRelease}, {0, EventType::JobStart}, {2, EventType::JobRelease},
        {2, EventType::JobPreempt}, {2, EventType::JobStart}, {4, EventType::JobFinish},
        {4, EventType::JobResume},  {8, EventType::JobFinish}};
    const auto& low = engine.scheduler().jobs()[0];
    const auto& high = engine.scheduler().jobs()[1];
    const auto accounting_matches =
        trace_shape(engine) == expected && low.id() == JobId{1} && low.first_start_tick() == 0 &&
        low.finish_tick() == 8 && low.preemption_count() == 1 && low.remaining_execution() == 0 &&
        high.first_start_tick() == 2 && high.finish_tick() == 4 && high.preemption_count() == 0;

    REQUIRE(accounting_matches);
}

/*** Verifies that configured non-preemptive mode keeps the Running job. ***/
TEST_CASE("non-preemptive fixed priority waits for the running job to finish",
          "[kernel][engine][non-preemptive]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    const auto config =
        make_config({{1, 100, 20, 0, 5, 2}, {2, 100, 20, 2, 1, 1}}, PreemptionMode::NonPreemptive);
    SimulationEngine engine{config, 6, allocator, policy};
    engine.run();

    const std::vector<std::pair<Tick, EventType>> expected{
        {0, EventType::JobRelease}, {0, EventType::JobStart}, {2, EventType::JobRelease},
        {5, EventType::JobFinish},  {5, EventType::JobStart}, {6, EventType::JobFinish}};
    const auto& runtime_scheduler = engine.scheduler();
    const auto behavior_matches =
        trace_shape(engine) == expected &&
        engine.scheduling().preemption_mode == PreemptionMode::NonPreemptive &&
        runtime_scheduler.preemption_mode() == PreemptionMode::NonPreemptive &&
        runtime_scheduler.jobs()[0].preemption_count() == 0 &&
        runtime_scheduler.jobs()[1].first_start_tick() == 5;
    REQUIRE(behavior_matches);
}

/*** Verifies that equal priority waits instead of preempting the running job. ***/
TEST_CASE("equal-priority jobs do not preempt", "[kernel][engine]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_config({{1, 100, 20, 0, 5, 1}, {2, 100, 20, 2, 1, 1}}), 8,
                            allocator, policy};
    engine.run();

    const std::vector<std::pair<Tick, EventType>> expected{
        {0, EventType::JobRelease}, {0, EventType::JobStart}, {2, EventType::JobRelease},
        {5, EventType::JobFinish},  {5, EventType::JobStart}, {6, EventType::JobFinish}};
    const auto no_preemption = trace_shape(engine) == expected &&
                               engine.scheduler().jobs()[0].preemption_count() == 0 &&
                               engine.scheduler().jobs()[1].first_start_tick() == 5;
    REQUIRE(no_preemption);
}

/*** Verifies deadline misses and completion-before-deadline-check semantics. ***/
TEST_CASE("deadline checks distinguish late jobs from exact completion",
          "[kernel][engine][deadline]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine late_engine{make_config({{1, 100, 10, 0, 4, 1}, {2, 100, 3, 0, 2, 2}}), 6,
                                 allocator, policy};
    late_engine.run();
    const auto late_shape = trace_shape(late_engine);
    const auto deadline_event = std::pair<Tick, EventType>{3, EventType::DeadlineMiss};
    const auto late_matches =
        late_engine.scheduler().jobs()[1].deadline_missed() &&
        late_engine.scheduler().jobs()[1].finish_tick() == 6 &&
        std::find(late_shape.begin(), late_shape.end(), deadline_event) != late_shape.end();
    REQUIRE(late_matches);

    SimulationEngine exact_engine{make_config({{1, 100, 3, 0, 3, 1}}), 3, allocator, policy};
    exact_engine.run();
    const auto exact_matches = !exact_engine.scheduler().jobs()[0].deadline_missed() &&
                               exact_engine.scheduler().jobs()[0].finish_tick() == 3 &&
                               exact_engine.trace().back().type() == EventType::JobFinish;
    REQUIRE(exact_matches);
}

/*** Verifies the explicit baseline rejection of self-overlapping task jobs. ***/
TEST_CASE("self-overlapping periodic jobs require a future overrun policy", "[kernel][engine]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_config({{1, 3, 5, 0, 5, 1}}), 6, allocator, policy};
    REQUIRE_THROWS_AS(engine.run(), std::logic_error);
}

/*** Verifies that a complete preemptive run produces identical JSON bytes. ***/
TEST_CASE("fixed-priority processed traces are repeatable", "[kernel][engine][trace]") {
    const auto config = make_config({{1, 100, 20, 0, 6, 2}, {2, 100, 10, 2, 2, 1}});
    const auto first = serialized_trace(config, 10);
    const auto second = serialized_trace(config, 10);
    const auto non_preemptive_config =
        make_config({{1, 100, 20, 0, 5, 2}, {2, 100, 20, 2, 1, 1}}, PreemptionMode::NonPreemptive);
    const auto non_preemptive_first = serialized_trace(non_preemptive_config, 6);
    const auto non_preemptive_second = serialized_trace(non_preemptive_config, 6);
    const bool traces_match = first == second && non_preemptive_first == non_preemptive_second;
    REQUIRE(traces_match);
}

/*** Verifies that runtime Scheduler obtains job choices through its policy. ***/
TEST_CASE("simulation engine accepts a different scheduling policy", "[kernel][engine][policy]") {
    const SingleResourceAllocator allocator;
    LastReadyPolicy policy;
    SimulationEngine engine{make_config({{1, 100, 20, 0, 1, 1}, {2, 100, 20, 0, 1, 1}}), 2,
                            allocator, policy};
    engine.run();

    const auto first_start =
        std::find_if(engine.trace().begin(), engine.trace().end(), [](const cpssim::Event& event) {
            return event.type() == EventType::JobStart;
        });
    const bool injected_policy_was_used =
        first_start != engine.trace().end() && first_start->entities().task_id == TaskId{2};
    REQUIRE(injected_policy_was_used);
}

/*** Verifies observations reach policy state before same-tick job selection. ***/
TEST_CASE("functional observation influences a later scheduling decision",
          "[kernel][engine][functional][policy]") {
    const SingleResourceAllocator allocator;
    ObservationAwarePolicy policy;
    cpssim::MockFunctionalModel functional_model;
    SimulationEngine engine{make_config({{1, 100, 20, 0, 1, 1}, {2, 100, 20, 0, 1, 1}}), 2,
                            allocator, policy, functional_model};
    engine.run();

    const auto first_start =
        std::find_if(engine.trace().begin(), engine.trace().end(), [](const cpssim::Event& event) {
            return event.type() == EventType::JobStart;
        });
    const bool observed_before_selection =
        first_start != engine.trace().end() && first_start->entities().task_id == TaskId{2} &&
        policy.observation_count() == 3 && engine.functional_trace().size() == 3;
    REQUIRE(observed_before_selection);
}

/*** Verifies that incremental stepping processes one complete event tick. ***/
TEST_CASE("simulation engine steps through complete logical event ticks",
          "[kernel][engine][step]") {
    const SingleResourceAllocator allocator;
    FixedPriorityPolicy policy;
    SimulationEngine engine{make_config({{1, 10, 10, 0, 3, 1}}), 10, allocator, policy};

    REQUIRE_FALSE(engine.finished());
    REQUIRE(engine.step_to_next_event());
    REQUIRE((engine.current_tick() == 0));
    REQUIRE((trace_shape(engine) == std::vector<std::pair<Tick, EventType>>{
                                        {0, EventType::JobRelease}, {0, EventType::JobStart}}));

    REQUIRE(engine.step_to_next_event());
    REQUIRE((engine.current_tick() == 3));
    REQUIRE((engine.trace().back().type() == EventType::JobFinish));

    REQUIRE(engine.step_to_next_event());
    REQUIRE((engine.current_tick() == 10));
    REQUIRE(engine.finished());
    REQUIRE_FALSE(engine.step_to_next_event());
}

/*** Verifies that the engine rejects incomplete allocator output before run. ***/
TEST_CASE("simulation engine validates the complete resource allocation plan",
          "[kernel][engine][allocator]") {
    const IncompleteAllocator incomplete_allocator;
    const DuplicateAllocator duplicate_allocator;
    FixedPriorityPolicy policy;
    const auto config = make_config({{1, 100, 20, 0, 1, 1}, {2, 100, 20, 0, 1, 1}});

    REQUIRE_THROWS_AS((SimulationEngine{config, 2, incomplete_allocator, policy}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((SimulationEngine{config, 2, duplicate_allocator, policy}),
                      std::invalid_argument);
}

} // namespace
