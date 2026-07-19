/***
 * File: tests/policy/fixed_priority_test.cpp
 * Purpose: Verify deterministic fixed-priority selection, stable tie breaking,
 *          strict preemption, and inconsistent read-only view rejection.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/policy/fixed_priority.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using cpssim::FixedPriorityPolicy;
using cpssim::JobId;
using cpssim::JobState;
using cpssim::JobTiming;
using cpssim::Resource;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::TaskId;
using cpssim::Tick;

/*** Creates one Ready policy fixture with explicit ordering fields. ***/
JobState make_job(std::uint64_t task_id, std::uint64_t job_id, cpssim::Priority priority,
                  Tick release_tick) {
    return JobState{
        JobId{job_id}, TaskId{task_id}, ResourceId{1}, priority,
        JobTiming{.release = release_tick, .absolute_deadline = release_tick + 20, .execution = 1}};
}

/*** Verifies priority first and all three deterministic tie breakers. ***/
TEST_CASE("fixed-priority selection uses the complete deterministic key", "[policy][fp]") {
    std::vector<JobState> jobs;
    jobs.reserve(5);
    jobs.push_back(make_job(2, 1, 1, 0));
    jobs.push_back(make_job(1, 2, 1, 0));
    jobs.push_back(make_job(1, 1, 1, 0));
    jobs.push_back(make_job(3, 1, 1, 1));
    jobs.push_back(make_job(4, 1, 2, 0));

    const Resource resource{ResourceSpec{ResourceId{1}, "cpu"}};
    std::vector<cpssim::JobIdentity> ready_jobs;
    ready_jobs.reserve(jobs.size() + 1);
    for (const auto& job : jobs) {
        ready_jobs.push_back(job.identity());
    }

    const FixedPriorityPolicy policy;
    const auto selected = policy.select(resource, ready_jobs, jobs);
    const auto tie_break_matches = selected.task_id() == TaskId{1} && selected.job_id() == JobId{1};
    REQUIRE(tie_break_matches);

    jobs.push_back(make_job(5, 1, 0, 10));
    ready_jobs.push_back(jobs.back().identity());
    const auto higher_priority = policy.select(resource, ready_jobs, jobs);
    const bool priority_dominates_tie_breakers = higher_priority.task_id() == TaskId{5};
    REQUIRE(priority_dominates_tie_breakers);
}

/*** Verifies equal priority does not preempt and inconsistent views fail. ***/
TEST_CASE("fixed-priority preemption is strict and scheduler views are validated", "[policy][fp]") {
    const auto running = make_job(1, 1, 1, 0);
    const auto equal_ready = make_job(2, 1, 1, 1);
    const auto higher_ready = make_job(3, 1, 0, 2);
    const FixedPriorityPolicy policy;

    REQUIRE_FALSE(policy.should_preempt(running, equal_ready));
    REQUIRE(policy.should_preempt(running, higher_ready));

    const Resource empty_resource{ResourceSpec{ResourceId{1}, "empty"}};
    REQUIRE_THROWS_AS(policy.select(empty_resource, {}, {}), std::logic_error);

    const auto absent_identity = make_job(9, 1, 1, 0).identity();
    REQUIRE_THROWS_AS(policy.select(empty_resource, {absent_identity}, {}), std::logic_error);
}

} // namespace
