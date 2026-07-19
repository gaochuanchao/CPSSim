/***
 * File: tests/kernel/periodic_release_test.cpp
 * Purpose: Verify task-owned assignment, runtime job generation, incremental
 *          releases, task-local job IDs, and deterministic output.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/kernel/periodic_release.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using cpssim::Event;
using cpssim::EventEntityRefs;
using cpssim::EventPhase;
using cpssim::EventSequence;
using cpssim::EventType;
using cpssim::ExperimentConfig;
using cpssim::JobId;
using cpssim::PeriodicReleaseModel;
using cpssim::PeriodicTimingSpec;
using cpssim::Priority;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::serialize_event_json_line;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;
using cpssim::Tick;

/*** Creates one valid periodic task without selecting its runtime resource. ***/
TaskSpec make_task(std::uint64_t id, Tick period, Tick offset, Priority priority) {
    return TaskSpec{TaskId{id}, "task_" + std::to_string(id),
                    PeriodicTimingSpec{.period = period, .deadline = period, .offset = offset},
                    priority};
}

/*** Wraps task fixtures and their valid resource choices in a configuration. ***/
ExperimentConfig
make_config(std::vector<TaskSpec> tasks,
            std::vector<TaskResourceProfile> profiles = {
                {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1}}) {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu_1"}, ResourceSpec{ResourceId{2}, "cpu_2"}},
        std::move(tasks),
        std::move(profiles)};
}

/*** Assigns every supplied task to resource 1 for release-only trace tests. ***/
void assign_to_first_resource(PeriodicReleaseModel& releases, const std::vector<TaskId>& task_ids) {
    for (const auto task_id : task_ids) {
        releases.assign_resource(task_id, ResourceId{1});
    }
}

/*** Processes a release-only run and returns its dynamically appended JSON. ***/
std::string run_release_only(const ExperimentConfig& config, Tick stop_tick,
                             const std::vector<TaskId>& task_ids) {
    PeriodicReleaseModel releases{config, stop_tick};
    assign_to_first_resource(releases, task_ids);
    cpssim::EventQueue queue;
    std::string output;
    releases.schedule_initial_releases(queue);

    while (!queue.empty()) {
        const auto event = queue.pop_next();
        output += serialize_event_json_line(event);
        releases.release(event, queue);
    }
    return output;
}

/*** Verifies that a task creates one job and successor at each release. ***/
TEST_CASE("a periodic task generates one job and release at a time", "[kernel][release]") {
    const auto config =
        make_config({make_task(4, 4, 2, 3)},
                    {{.task_id = TaskId{4}, .resource_id = ResourceId{2}, .execution_time = 2}});
    PeriodicReleaseModel releases{config, 10};
    releases.assign_resource(TaskId{4}, ResourceId{2});
    cpssim::EventQueue queue;

    const auto one_initial_release = releases.schedule_initial_releases(queue) == 1;
    REQUIRE(one_initial_release);
    const auto first_event = queue.pop_next();
    const auto first_job = releases.release(first_event, queue);
    const auto first_matches =
        first_job.task_id() == TaskId{4} && first_job.id() == JobId{1} &&
        first_job.resource_id() == ResourceId{2} && first_job.release_tick() == 2 &&
        first_job.absolute_deadline() == 6 && first_job.remaining_execution() == 2;
    REQUIRE(first_matches);

    const auto second_event = queue.pop_next();
    const auto second_job = releases.release(second_event, queue);
    const auto second_matches = second_job.id() == JobId{2} && second_job.release_tick() == 6;
    REQUIRE(second_matches);

    const auto third_event = queue.pop_next();
    const auto third_job = releases.release(third_event, queue);
    const auto third_matches = third_job.id() == JobId{3} && third_job.release_tick() == 10;
    REQUIRE(third_matches);
    REQUIRE(queue.empty());
}

/*** Verifies deterministic first releases and independent task-local numbering. ***/
TEST_CASE("each periodic task starts its own job sequence at one", "[kernel][release]") {
    const auto config =
        make_config({make_task(3, 8, 0, 2), make_task(2, 6, 0, 1), make_task(1, 4, 0, 1)},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1},
                     {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 1},
                     {.task_id = TaskId{3}, .resource_id = ResourceId{1}, .execution_time = 1}});
    PeriodicReleaseModel releases{config, 0};
    const std::vector<TaskId> expected_tasks{TaskId{1}, TaskId{2}, TaskId{3}};
    assign_to_first_resource(releases, expected_tasks);
    cpssim::EventQueue queue;

    const auto initial_count_matches =
        releases.schedule_initial_releases(queue) == expected_tasks.size();
    REQUIRE(initial_count_matches);
    for (std::uint64_t index = 0; index < expected_tasks.size(); ++index) {
        const auto event = queue.pop_next();
        const auto identity_matches =
            event.tick() == 0 && event.entities().task_id == expected_tasks.at(index) &&
            event.entities().job_id == JobId{1} && event.sequence() == EventSequence{index};
        REQUIRE(identity_matches);
        const auto job = releases.release(event, queue);
        const auto job_number_matches = job.id() == JobId{1};
        REQUIRE(job_number_matches);
    }
    REQUIRE(queue.empty());
}

/*** Verifies required, accessible task-level resource assignment. ***/
TEST_CASE("a task must receive an accessible resource before release", "[kernel][release]") {
    const auto config = make_config({make_task(1, 10, 0, 1)});
    PeriodicReleaseModel releases{config, 10};
    cpssim::EventQueue queue;

    REQUIRE_THROWS_AS(releases.assign_resource(TaskId{1}, ResourceId{2}), std::invalid_argument);
    REQUIRE_THROWS_AS(releases.assign_resource(TaskId{99}, ResourceId{1}), std::logic_error);
    REQUIRE_THROWS_AS(releases.schedule_initial_releases(queue), std::logic_error);
    REQUIRE(queue.empty());
}

/*** Verifies assignment capture and per-resource execution demand. ***/
TEST_CASE("reassignment changes future jobs but not a pending job", "[kernel][release]") {
    const auto config =
        make_config({make_task(1, 10, 0, 1)},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 3},
                     {.task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 7}});
    PeriodicReleaseModel releases{config, 10};
    cpssim::EventQueue queue;

    releases.assign_resource(TaskId{1}, ResourceId{1});
    releases.schedule_initial_releases(queue);
    const auto first_event = queue.pop_next();
    releases.assign_resource(TaskId{1}, ResourceId{2});

    const auto first_job = releases.release(first_event, queue);
    const auto second_event = queue.pop_next();
    const auto second_job = releases.release(second_event, queue);
    const auto assignments_match =
        first_event.entities().resource_id == ResourceId{1} &&
        first_job.resource_id() == ResourceId{1} && first_job.remaining_execution() == 3 &&
        second_event.entities().resource_id == ResourceId{2} &&
        second_job.resource_id() == ResourceId{2} && second_job.remaining_execution() == 7;

    REQUIRE(assignments_match);
    const auto task_keeps_new_assignment =
        releases.task(TaskId{1}).assigned_resource() == ResourceId{2};
    REQUIRE(task_keeps_new_assignment);
}

/*** Verifies that offsets beyond the horizon create no event or assignment need. ***/
TEST_CASE("periodic tasks skip first releases beyond the horizon", "[kernel][release]") {
    const auto config = make_config({make_task(1, 10, 11, 1)});
    PeriodicReleaseModel releases{config, 10};
    cpssim::EventQueue queue;

    const auto no_initial_release = releases.schedule_initial_releases(queue) == 0;
    REQUIRE(no_initial_release);
    REQUIRE(queue.empty());
}

/*** Verifies initialization and processed-release protocol failures. ***/
TEST_CASE("periodic release state rejects invalid progression", "[kernel][release]") {
    const auto config = make_config({make_task(1, 10, 0, 1)});
    REQUIRE_THROWS_AS((PeriodicReleaseModel{config, -1}), std::invalid_argument);

    PeriodicReleaseModel releases{config, 20};
    cpssim::EventQueue queue;
    const Event unrelated{0, EventPhase::Scheduling, EventSequence{50}, EventType::JobStart};
    REQUIRE_THROWS_AS(releases.release(unrelated, queue), std::logic_error);

    releases.assign_resource(TaskId{1}, ResourceId{1});
    releases.schedule_initial_releases(queue);
    REQUIRE_THROWS_AS(releases.schedule_initial_releases(queue), std::logic_error);
    REQUIRE_THROWS_AS(releases.release(unrelated, queue), std::logic_error);

    const Event missing_identity{0, EventPhase::JobRelease, EventSequence{51},
                                 EventType::JobRelease};
    REQUIRE_THROWS_AS(releases.release(missing_identity, queue), std::logic_error);

    const Event unknown_task{0, EventPhase::JobRelease, EventSequence{52}, EventType::JobRelease,
                             EventEntityRefs{.task_id = TaskId{99},
                                             .job_id = JobId{1},
                                             .resource_id = ResourceId{1},
                                             .message_id = std::nullopt,
                                             .vehicle_id = std::nullopt}};
    REQUIRE_THROWS_AS(releases.release(unknown_task, queue), std::logic_error);

    const auto first = queue.pop_next();
    releases.release(first, queue);
    REQUIRE_THROWS_AS(releases.release(first, queue), std::logic_error);
}

/*** Verifies safe release progression near the largest canonical Tick. ***/
TEST_CASE("incremental periodic release arithmetic does not overflow Tick", "[kernel][release]") {
    const auto largest_tick = std::numeric_limits<Tick>::max();
    const auto config =
        make_config({make_task(1, 1, largest_tick - 2, 1)},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1}});
    PeriodicReleaseModel releases{config, largest_tick - 1};
    releases.assign_resource(TaskId{1}, ResourceId{1});
    cpssim::EventQueue queue;

    releases.schedule_initial_releases(queue);
    const auto first = queue.pop_next();
    releases.release(first, queue);
    const auto final_event = queue.pop_next();
    const auto final_job = releases.release(final_event, queue);
    const auto boundary_matches = final_job.release_tick() == largest_tick - 1 &&
                                  final_job.absolute_deadline() == largest_tick;
    REQUIRE(boundary_matches);
    REQUIRE(queue.empty());
}

/*** Verifies repeatable JSON appended in release-processing order. ***/
TEST_CASE("runtime periodic release output is repeatable", "[kernel][release][trace]") {
    const auto config =
        make_config({make_task(2, 10, 0, 2), make_task(1, 10, 0, 1)},
                    {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1},
                     {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 1}});
    const std::vector<TaskId> task_ids{TaskId{1}, TaskId{2}};
    const auto first_output = run_release_only(config, 10, task_ids);
    const auto second_output = run_release_only(config, 10, task_ids);
    const std::string expected =
        R"json({"schema_version":1,"tick":0,"phase":"job_release","sequence":0,"type":"job_release","entities":{"task_id":1,"job_id":1,"resource_id":1,"message_id":null,"vehicle_id":null},"cause_sequence":null})json"
        "\n"
        R"json({"schema_version":1,"tick":0,"phase":"job_release","sequence":1,"type":"job_release","entities":{"task_id":2,"job_id":1,"resource_id":1,"message_id":null,"vehicle_id":null},"cause_sequence":null})json"
        "\n"
        R"json({"schema_version":1,"tick":10,"phase":"job_release","sequence":2,"type":"job_release","entities":{"task_id":1,"job_id":2,"resource_id":1,"message_id":null,"vehicle_id":null},"cause_sequence":null})json"
        "\n"
        R"json({"schema_version":1,"tick":10,"phase":"job_release","sequence":3,"type":"job_release","entities":{"task_id":2,"job_id":2,"resource_id":1,"message_id":null,"vehicle_id":null},"cause_sequence":null})json"
        "\n";

    const auto expected_bytes_match = first_output == expected;
    const auto repeated_bytes_match = second_output == first_output;
    REQUIRE(expected_bytes_match);
    REQUIRE(repeated_bytes_match);
}

} // namespace
