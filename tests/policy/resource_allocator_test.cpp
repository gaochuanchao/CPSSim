/***
 * File: tests/policy/resource_allocator_test.cpp
 * Purpose: Verify the common resource-allocation interface and the T9
 *          single-resource allocation strategy.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/policy/resource_allocator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

using cpssim::ConfiguredResourceAllocator;
using cpssim::ExperimentConfig;
using cpssim::PeriodicTimingSpec;
using cpssim::PreemptionMode;
using cpssim::ResourceAllocator;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::SingleResourceAllocator;
using cpssim::TaskAssignment;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;

/*** Creates two tasks that can execute on one resource. ***/
ExperimentConfig
make_single_resource_config(PreemptionMode preemption_mode = PreemptionMode::Preemptive) {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = preemption_mode},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "first", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1},
         TaskSpec{TaskId{2}, "second",
                  PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2}},
        {TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1},
         TaskResourceProfile{
             .task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 2}}};
}

/*** Test allocator that demonstrates access to the shared scheduling assumption. ***/
class PreemptionAwareAllocator : public ResourceAllocator {
  public:
    std::vector<TaskAssignment> allocate(const ExperimentConfig& config) const override {
        if (config.scheduling().preemption_mode != PreemptionMode::NonPreemptive) {
            throw std::invalid_argument{"test allocator expected non-preemptive scheduling"};
        }
        return {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                {.task_id = TaskId{2}, .resource_id = ResourceId{1}}};
    }
};

/*** Verifies one complete deterministic plan through the base interface. ***/
TEST_CASE("single-resource allocator assigns every task to the only resource",
          "[policy][allocator]") {
    const SingleResourceAllocator single_resource_allocator;
    const ResourceAllocator& allocator = single_resource_allocator;
    const auto assignments = allocator.allocate(make_single_resource_config());

    const bool plan_matches = assignments.size() == 2 && assignments[0].task_id == TaskId{1} &&
                              assignments[0].resource_id == ResourceId{1} &&
                              assignments[1].task_id == TaskId{2} &&
                              assignments[1].resource_id == ResourceId{1};
    REQUIRE(plan_matches);
}

/*** Verifies that the T9 allocator refuses a multi-resource decision. ***/
TEST_CASE("single-resource allocator rejects multiple configured resources",
          "[policy][allocator]") {
    const ExperimentConfig config{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = cpssim::PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "local"}, ResourceSpec{ResourceId{2}, "cloud"}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1},
         TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 2}}};
    const SingleResourceAllocator allocator;

    REQUIRE_THROWS_AS(allocator.allocate(config), std::invalid_argument);
}

/*** Verifies that an explicit plan is retained behind the common interface. ***/
TEST_CASE("configured resource allocator returns the supplied task mapping",
          "[policy][allocator]") {
    const std::vector<TaskAssignment> supplied{
        {.task_id = TaskId{1}, .resource_id = ResourceId{2}},
        {.task_id = TaskId{2}, .resource_id = ResourceId{1}}};
    const ConfiguredResourceAllocator configured_allocator{supplied};
    const ResourceAllocator& allocator = configured_allocator;
    const auto returned = allocator.allocate(make_single_resource_config());

    const bool mapping_matches = returned.size() == 2 && returned[0].task_id == TaskId{1} &&
                                 returned[0].resource_id == ResourceId{2} &&
                                 returned[1].task_id == TaskId{2} &&
                                 returned[1].resource_id == ResourceId{1};
    REQUIRE(mapping_matches);
}

/*** Verifies allocators can use the same preemption mode as runtime scheduling. ***/
TEST_CASE("resource allocator receives experiment scheduling assumptions",
          "[policy][allocator][scheduling]") {
    const PreemptionAwareAllocator allocator;
    const auto assignments =
        allocator.allocate(make_single_resource_config(PreemptionMode::NonPreemptive));
    const bool complete_plan = assignments.size() == 2;
    REQUIRE(complete_plan);
}

} // namespace
