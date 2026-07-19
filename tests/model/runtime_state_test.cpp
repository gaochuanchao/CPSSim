/***
 * File: tests/model/runtime_state_test.cpp
 * Purpose: Verify JobState lifecycle data and Resource-owned Running execution,
 *          completion expectations, preemption, and integer utilization data.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/runtime_state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

namespace {

using cpssim::JobId;
using cpssim::JobIdentity;
using cpssim::JobLifecycle;
using cpssim::JobState;
using cpssim::JobTiming;
using cpssim::Resource;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::TaskId;

/*** Creates a valid six-tick test job with identity/resource overrides. ***/
JobState make_job(JobId id = JobId{1}, ResourceId resource_id = ResourceId{1},
                  TaskId task_id = TaskId{10}) {
    return JobState{id, task_id, resource_id, 2,
                    JobTiming{.release = 5, .absolute_deadline = 15, .execution = 6}};
}

/*** Verifies initial Ready job values and an idle zero-busy resource. ***/
TEST_CASE("a new job is ready and its resource starts idle", "[model][runtime]") {
    const auto job = make_job();
    const Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};

    const auto job_matches = job.id() == JobId{1} && job.task_id() == TaskId{10} &&
                             job.resource_id() == ResourceId{1} && job.release_tick() == 5 &&
                             job.absolute_deadline() == 15 && job.remaining_execution() == 6 &&
                             job.lifecycle() == JobLifecycle::Ready && !job.has_started();
    const auto resource_matches =
        resource.id() == ResourceId{1} && resource.spec().name() == "cpu" &&
        !resource.running_job().has_value() && !resource.running_since().has_value() &&
        !resource.expected_completion_tick().has_value() && resource.busy_ticks_until(5) == 0 &&
        resource.idle_ticks_until(5) == 5;

    REQUIRE(job_matches);
    REQUIRE(resource_matches);
}

/*** Verifies rejection of invalid release, deadline, and execution timing. ***/
TEST_CASE("job construction rejects invalid timing", "[model][runtime]") {
    REQUIRE_THROWS_AS((JobState{JobId{1}, TaskId{1}, ResourceId{1}, 1,
                                JobTiming{.release = -1, .absolute_deadline = 10, .execution = 1}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((JobState{JobId{1}, TaskId{1}, ResourceId{1}, 1,
                                JobTiming{.release = 10, .absolute_deadline = 10, .execution = 1}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((JobState{JobId{1}, TaskId{1}, ResourceId{1}, 1,
                                JobTiming{.release = 0, .absolute_deadline = 10, .execution = 0}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((JobState{JobId{1}, TaskId{1}, ResourceId{1}, 1,
                                JobTiming{.release = 0, .absolute_deadline = 10, .execution = 11}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((JobState{JobId{1}, TaskId{1}, ResourceId{1}, -1,
                                JobTiming{.release = 0, .absolute_deadline = 10, .execution = 1}}),
                      std::invalid_argument);
}

/*** Verifies that a selected Ready job starts one tracked execution interval. ***/
TEST_CASE("resource starts a scheduler-selected ready job", "[model][runtime]") {
    auto job = make_job();
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};

    resource.start_job(job, 5);
    const auto running_job = resource.running_job();
    const auto started = job.lifecycle() == JobLifecycle::Running && job.has_started() &&
                         job.first_start_tick() == 5 && resource.running_since() == 5 &&
                         resource.expected_completion_tick() == 11 &&
                         running_job == std::optional<JobIdentity>{job.identity()};
    REQUIRE(started);
}

/*** Verifies preemption accounting without giving Resource a Ready queue. ***/
TEST_CASE("resource preemption accounts execution and returns the job ready", "[model][runtime]") {
    auto job = make_job();
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};

    resource.start_job(job, 5);
    resource.preempt_job(job, 7);
    const auto preempted = job.lifecycle() == JobLifecycle::Ready && job.has_started() &&
                           job.remaining_execution() == 4 && job.preemption_count() == 1 &&
                           !resource.running_job().has_value() &&
                           !resource.running_since().has_value() &&
                           !resource.expected_completion_tick().has_value() &&
                           resource.busy_ticks_until(7) == 2 && resource.idle_ticks_until(7) == 5;
    REQUIRE(preempted);

    resource.start_job(job, 8);
    const auto resumed = job.lifecycle() == JobLifecycle::Running && job.first_start_tick() == 5 &&
                         resource.running_since() == 8 && resource.expected_completion_tick() == 12;
    REQUIRE(resumed);
}

/*** Verifies partial charging and completion exactly at expected demand. ***/
TEST_CASE("execution charging completes a job exactly at zero remaining work", "[model][runtime]") {
    auto job = make_job();
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};
    resource.start_job(job, 5);

    const auto completed_early = resource.charge_execution(job, 7);
    const auto partial = !completed_early && job.remaining_execution() == 4 &&
                         job.lifecycle() == JobLifecycle::Running &&
                         resource.running_since() == 7 && resource.busy_ticks_until(7) == 2;
    REQUIRE(partial);

    const auto completed = resource.charge_execution(job, 11);
    const auto final = completed && job.remaining_execution() == 0 &&
                       job.lifecycle() == JobLifecycle::Completed && job.finish_tick() == 11 &&
                       !resource.running_job().has_value() && resource.busy_ticks_until(11) == 6 &&
                       resource.idle_ticks_until(11) == 5;
    REQUIRE(final);
}

/*** Verifies complete task/job identity in Running resource membership. ***/
TEST_CASE("resource occupancy uses complete task and job identity", "[model][runtime]") {
    auto first_task_job = make_job(JobId{1}, ResourceId{1}, TaskId{10});
    auto second_task_job = make_job(JobId{1}, ResourceId{1}, TaskId{20});
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};

    resource.start_job(first_task_job, 5);
    const bool first_identity_matches =
        resource.running_job() == std::optional<JobIdentity>{first_task_job.identity()};
    REQUIRE(first_identity_matches);
    resource.charge_execution(first_task_job, 11);
    resource.start_job(second_task_job, 11);
    const bool second_identity_matches =
        resource.running_job() == std::optional<JobIdentity>{second_task_job.identity()};
    REQUIRE(second_identity_matches);
}

/*** Verifies forbidden operations fail without accepting inconsistent state. ***/
TEST_CASE("runtime transitions reject inconsistent or forbidden changes", "[model][runtime]") {
    auto job = make_job();
    auto other_job = make_job(JobId{2});
    auto wrong_resource_job = make_job(JobId{3}, ResourceId{2});
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};

    REQUIRE_THROWS_AS(resource.start_job(wrong_resource_job, 5), std::logic_error);
    REQUIRE_THROWS_AS(resource.start_job(job, 4), std::invalid_argument);
    resource.start_job(job, 5);
    REQUIRE_THROWS_AS(resource.start_job(other_job, 5), std::logic_error);
    REQUIRE_THROWS_AS(resource.charge_execution(job, 5), std::invalid_argument);
    REQUIRE_THROWS_AS(resource.charge_execution(job, 12), std::logic_error);
    REQUIRE_THROWS_AS(resource.preempt_job(job, 11), std::logic_error);
    REQUIRE_THROWS_AS(resource.busy_ticks_until(-1), std::invalid_argument);

    resource.charge_execution(job, 11);
    REQUIRE_THROWS_AS(resource.preempt_job(job, 12), std::logic_error);
    REQUIRE_THROWS_AS(resource.start_job(job, 12), std::logic_error);
    REQUIRE_THROWS_AS(resource.charge_execution(job, 12), std::logic_error);
}

/*** Verifies start, finish, preemption, and deadline history on JobState. ***/
TEST_CASE("runtime jobs record start finish preemption and deadline outcomes", "[model][runtime]") {
    auto completed_job = make_job();
    Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};
    resource.start_job(completed_job, 5);
    resource.preempt_job(completed_job, 7);
    resource.start_job(completed_job, 7);
    resource.charge_execution(completed_job, 11);

    const auto history_matches =
        completed_job.priority() == 2 && completed_job.first_start_tick() == 5 &&
        completed_job.finish_tick() == 11 && completed_job.preemption_count() == 1 &&
        !completed_job.mark_deadline_missed(15) && !completed_job.deadline_missed();
    REQUIRE(history_matches);

    auto late_job = make_job(JobId{2});
    REQUIRE(late_job.mark_deadline_missed(15));
    REQUIRE(late_job.deadline_missed());
    REQUIRE_THROWS_AS(late_job.mark_deadline_missed(15), std::logic_error);
    REQUIRE_THROWS_AS(make_job(JobId{3}).mark_deadline_missed(14), std::invalid_argument);
}

} // namespace
