/***
 * File: tests/model/identifiers_test.cpp
 * Purpose: Verify compile-time separation and value behavior of the strong
 *          simulator identifier classes.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/identifiers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <type_traits>

namespace {

using cpssim::EventSequence;
using cpssim::JobId;
using cpssim::JobIdentity;
using cpssim::MessageId;
using cpssim::ResourceId;
using cpssim::TaskId;
using cpssim::VehicleId;

static_assert(!std::is_convertible_v<std::uint64_t, TaskId>);
static_assert(!std::is_same_v<TaskId, JobId>);
static_assert(!std::is_same_v<ResourceId, VehicleId>);

/*** Verifies that each identifier domain stores its value without type mixing. ***/
TEST_CASE("identifier domains remain distinct value types", "[model][identifier]") {
    const TaskId task_id{7};
    const JobId job_id{7};
    const ResourceId resource_id{7};
    const MessageId message_id{7};
    const VehicleId vehicle_id{7};
    const EventSequence event_sequence{7};
    const auto values_match = task_id.value() == 7 && job_id.value() == 7 &&
                              resource_id.value() == 7 && message_id.value() == 7 &&
                              vehicle_id.value() == 7 && event_sequence.value() == 7;

    REQUIRE(values_match);
}

/*** Verifies equality and ordering based on an identifier's stored number. ***/
TEST_CASE("identifiers compare by their stored value", "[model][identifier]") {
    const TaskId lower{2};
    const TaskId same{2};
    const TaskId higher{9};
    const auto equal_values_match = lower == same;
    const auto ordered_values_match = lower < higher;

    REQUIRE(equal_values_match);
    REQUIRE(ordered_values_match);
}

/*** Verifies that task-local job numbers form distinct complete identities. ***/
TEST_CASE("job identity combines task and task-local job IDs", "[model][identifier]") {
    const JobIdentity first_task_job{TaskId{1}, JobId{1}};
    const JobIdentity second_task_job{TaskId{2}, JobId{1}};
    const JobIdentity first_task_next_job{TaskId{1}, JobId{2}};
    const auto fields_match =
        first_task_job.task_id() == TaskId{1} && first_task_job.job_id() == JobId{1};
    const auto identities_differ =
        first_task_job != second_task_job && first_task_job != first_task_next_job;

    REQUIRE(fields_match);
    REQUIRE(identities_differ);
}

} // namespace
