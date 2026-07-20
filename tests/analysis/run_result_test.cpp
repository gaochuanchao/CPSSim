/*** Verify immutable generic run-result derivation and range projection. ***/

#include "cpssim/analysis/completed_run_result.hpp"
#include "cpssim/analysis/run_result.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace {

using namespace cpssim;
using namespace std::chrono_literals;

Event event(Tick tick, EventSequence sequence, EventType type, EventEntityRefs entities = {}) {
    return Event{tick, EventPhase::Scheduling, sequence, type, std::move(entities)};
}

EventEntityRefs job_refs(TaskId task_id, JobId job_id) {
    EventEntityRefs refs;
    refs.task_id = task_id;
    refs.job_id = job_id;
    return refs;
}

EventEntityRefs message_refs(MessageId message_id) {
    EventEntityRefs refs;
    refs.message_id = message_id;
    return refs;
}

SimulationSnapshot snapshot() {
    SimulationSnapshot value{.run_state = GuiRunState::Finished,
                             .current_tick = 12,
                             .stop_tick = 12,
                             .experiment = {},
                             .event_log = {},
                             .functional_model_attached = true,
                             .functional_signal_registry = {},
                             .functional_observations = {},
                             .resources = {}};
    value.experiment.tick_period = 2ms;
    value.experiment.tasks = {{TaskId{2}, "task two", 10, 10, 0, Priority{1}},
                              {TaskId{7}, "task seven", 10, 10, 0, Priority{2}}};
    value.resources = {{ResourceId{9}, "CPU", std::nullopt, {}, 8, 4},
                       {ResourceId{3}, "unused", std::nullopt, {}, 0, 0}};
    value.event_log = {
        event(1, EventSequence{0}, EventType::JobRelease, job_refs(TaskId{2}, JobId{0})),
        event(2, EventSequence{1}, EventType::MessageSend, message_refs(MessageId{4})),
        event(4, EventSequence{2}, EventType::JobPreempt, job_refs(TaskId{2}, JobId{0})),
        event(6, EventSequence{3}, EventType::JobFinish, job_refs(TaskId{2}, JobId{0})),
        event(7, EventSequence{4}, EventType::DeadlineMiss, job_refs(TaskId{7}, JobId{0})),
        event(9, EventSequence{5}, EventType::MessageDelivery, message_refs(MessageId{4})),
        event(10, EventSequence{6}, EventType::JobFinish, job_refs(TaskId{7}, JobId{9})),
    };
    return value;
}

TEST_CASE("run metrics derive exact paired timing and explicit unavailable values",
          "[analysis][metrics]") {
    const auto metrics = derive_run_metrics(snapshot());
    REQUIRE(metrics.event_count == 7);
    REQUIRE(metrics.tick_period == 2ms);
    REQUIRE(metrics.horizon_tick == 12);
    REQUIRE(metrics.horizon_time == 24ms);
    REQUIRE(metrics.completed_jobs == 2);
    REQUIRE(metrics.deadline_misses == 1);
    REQUIRE(metrics.preemptions == 1);
    REQUIRE(metrics.task_responses.size() == 2);
    REQUIRE(metrics.task_responses[0].task_id == TaskId{2});
    REQUIRE(metrics.task_responses[0].response_time.has_value());
    REQUIRE(metrics.task_responses[0].response_time->minimum == 5);
    REQUIRE(metrics.task_responses[0].response_time->mean() == 5.0);
    REQUIRE_FALSE(metrics.task_responses[1].response_time.has_value());
    REQUIRE(metrics.messages.sent == 1);
    REQUIRE(metrics.messages.delivered == 1);
    REQUIRE(metrics.messages.delivery_delay->minimum == 7);
    REQUIRE(metrics.resources[0].resource_id == ResourceId{3});
    REQUIRE_FALSE(metrics.resources[0].utilization.has_value());
    REQUIRE(metrics.resources[1].resource_id == ResourceId{9});
    REQUIRE(metrics.resources[1].utilization.value() == Catch::Approx(2.0 / 3.0));
}

TEST_CASE("empty and incomplete traces remain deterministic", "[analysis][metrics]") {
    auto value = snapshot();
    value.current_tick = 0;
    value.event_log.clear();
    value.resources.clear();
    const auto first = derive_run_metrics(value);
    const auto second = derive_run_metrics(value);
    REQUIRE(first == second);
    REQUIRE(first.event_count == 0);
    REQUIRE(first.horizon_time == 0ns);
    REQUIRE_FALSE(first.task_responses[0].response_time.has_value());
    REQUIRE_FALSE(first.messages.delivery_delay.has_value());
}

TEST_CASE("selected result range is inclusive and preserves source data", "[analysis][range]") {
    const auto source = snapshot();
    const auto selected = select_run_result_range(source, 4, 7);
    REQUIRE(selected.event_log.size() == 3);
    REQUIRE(selected.event_log.front().tick() == 4);
    REQUIRE(selected.event_log.back().tick() == 7);
    REQUIRE(source.event_log.size() == 7);
    REQUIRE_THROWS_AS(select_run_result_range(source, -1, 2), std::invalid_argument);
}

TEST_CASE("completed results build once only after finish and invalidate explicitly",
          "[analysis][result][lifecycle]") {
    CompletedRunResultCache cache;
    auto value = snapshot();
    value.run_state = GuiRunState::Running;
    REQUIRE_FALSE(cache.publish_finished(1, value, "generic", {}));
    value.run_state = GuiRunState::Paused;
    REQUIRE_FALSE(cache.publish_finished(1, value, "generic", {}));
    REQUIRE(cache.build_count() == 0);
    value.run_state = GuiRunState::Finished;
    REQUIRE(cache.publish_finished(1, value, "generic", {}));
    REQUIRE_FALSE(cache.publish_finished(1, value, "generic", {}));
    REQUIRE(cache.build_count() == 1);
    REQUIRE(cache.get() != nullptr);
    cache.invalidate();
    REQUIRE(cache.get() == nullptr);
    REQUIRE(cache.publish_finished(2, value, "generic", {}));
    REQUIRE(cache.build_count() == 2);
}

} // namespace
