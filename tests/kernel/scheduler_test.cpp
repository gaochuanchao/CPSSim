/***
 * File: tests/kernel/scheduler_test.cpp
 * Purpose: Verify runtime Scheduler ownership of jobs and Ready queues while
 *          Resource owns only selected Running execution and accounting.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/kernel/scheduler.hpp"
#include "cpssim/policy/fixed_priority.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using cpssim::EventPhase;
using cpssim::EventQueue;
using cpssim::EventType;
using cpssim::FixedPriorityPolicy;
using cpssim::JobId;
using cpssim::JobIdentity;
using cpssim::JobLifecycle;
using cpssim::JobState;
using cpssim::JobTiming;
using cpssim::PreemptionMode;
using cpssim::Resource;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::Scheduler;
using cpssim::SchedulingSpec;
using cpssim::TaskId;

/*** Creates one scheduler-submission fixture for resource one. ***/
JobState make_job(TaskId task_id, cpssim::Priority priority, cpssim::Tick execution) {
    return JobState{JobId{1}, task_id, ResourceId{1}, priority,
                    JobTiming{.release = 0, .absolute_deadline = 20, .execution = execution}};
}

/*** Verifies that Scheduler owns Ready membership and dispatches through Resource. ***/
TEST_CASE("runtime scheduler owns submitted jobs and resource ready queues",
          "[kernel][scheduler]") {
    FixedPriorityPolicy policy;
    Scheduler scheduler{{ResourceSpec{ResourceId{1}, "cpu"}},
                        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                        policy,
                        20};
    EventQueue queue;

    scheduler.submit(make_job(TaskId{1}, 2, 5), queue);
    scheduler.submit(make_job(TaskId{2}, 1, 1), queue);
    const auto submitted = scheduler.jobs().size() == 2 &&
                           scheduler.ready_jobs(ResourceId{1}).size() == 2 &&
                           !scheduler.resource(ResourceId{1}).running_job().has_value();
    REQUIRE(submitted);

    scheduler.schedule(0, queue);
    const auto running_job = scheduler.resource(ResourceId{1}).running_job();
    const auto dispatched =
        scheduler.ready_jobs(ResourceId{1}).size() == 1 &&
        running_job == std::optional<JobIdentity>{JobIdentity{TaskId{2}, JobId{1}}};
    REQUIRE(dispatched);

    const auto start = queue.pop_next();
    const auto completion = queue.pop_next();
    const bool events_match =
        start.type() == EventType::JobStart && completion.type() == EventType::JobFinish;
    REQUIRE(events_match);
    REQUIRE(scheduler.process_completion(completion));
    scheduler.schedule(1, queue);

    const auto handoff = scheduler.jobs()[1].lifecycle() == JobLifecycle::Completed &&
                         scheduler.jobs()[0].lifecycle() == JobLifecycle::Running &&
                         scheduler.ready_jobs(ResourceId{1}).empty() &&
                         scheduler.resource(ResourceId{1}).busy_ticks_until(1) == 1;
    REQUIRE(handoff);
}

/*** Verifies scheduler construction and public lookup failure boundaries. ***/
TEST_CASE("runtime scheduler rejects invalid construction and resource lookup",
          "[kernel][scheduler]") {
    FixedPriorityPolicy policy;
    const auto scheduling = SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive};

    REQUIRE_THROWS_AS((Scheduler{{}, scheduling, policy, 10}), std::invalid_argument);
    REQUIRE_THROWS_AS((Scheduler{{ResourceSpec{ResourceId{1}, "cpu"}}, scheduling, policy, -1}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        (Scheduler{{ResourceSpec{ResourceId{1}, "cpu"}, ResourceSpec{ResourceId{1}, "duplicate"}},
                   scheduling,
                   policy,
                   10}),
        std::invalid_argument);

    const Scheduler scheduler{{ResourceSpec{ResourceId{1}, "cpu"}}, scheduling, policy, 10};
    REQUIRE_THROWS_AS(scheduler.resource(ResourceId{2}), std::logic_error);
    REQUIRE_THROWS_AS(scheduler.ready_jobs(ResourceId{2}), std::logic_error);
}

/*** Verifies that public submission and deadline APIs reject invalid state. ***/
TEST_CASE("runtime scheduler rejects invalid submitted jobs and event resources",
          "[kernel][scheduler]") {
    FixedPriorityPolicy policy;
    Scheduler scheduler{{ResourceSpec{ResourceId{1}, "cpu"}},
                        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                        policy,
                        20};
    EventQueue queue;

    auto completed_job = make_job(TaskId{1}, 1, 1);
    Resource external_resource{ResourceSpec{ResourceId{1}, "cpu"}};
    external_resource.start_job(completed_job, 0);
    external_resource.charge_execution(completed_job, 1);
    REQUIRE_THROWS_AS(scheduler.submit(completed_job, queue), std::logic_error);

    scheduler.submit(make_job(TaskId{2}, 1, 1), queue);
    EventQueue wrong_event_queue;
    wrong_event_queue.schedule(20, EventPhase::DeadlineCheck, EventType::DeadlineMiss,
                               {.task_id = TaskId{2},
                                .job_id = JobId{1},
                                .resource_id = ResourceId{2},
                                .message_id = std::nullopt,
                                .vehicle_id = std::nullopt});
    REQUIRE_THROWS_AS(scheduler.process_deadline(wrong_event_queue.pop_next()), std::logic_error);
    REQUIRE_FALSE(scheduler.jobs().front().deadline_missed());
}

} // namespace
