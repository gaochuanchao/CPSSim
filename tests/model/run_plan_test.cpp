/***
 * File: tests/model/run_plan_test.cpp
 * Purpose: Verify shared typed run-plan validation and canonicalization.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/model/run_plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>

namespace {

using namespace cpssim;

ExperimentConfig make_run_plan_config() {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "local"}, ResourceSpec{ResourceId{2}, "cloud"}},
        {TaskSpec{TaskId{2}, "second",
                  PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2},
         TaskSpec{TaskId{1}, "first", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1},
         TaskResourceProfile{
             .task_id = TaskId{1}, .resource_id = ResourceId{2}, .execution_time = 2},
         TaskResourceProfile{
             .task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 3}}};
}

bool has_code(const RunPlanBuildResult& result, RunPlanDiagnosticCode code) {
    return std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [code](const RunPlanDiagnostic& diagnostic) { return diagnostic.code == code; });
}

TEST_CASE("run plan builder accepts complete accessible assignments", "[run-plan][model]") {
    const RunPlanRequest request{
        .stop_tick = 0,
        .policy_kind = SchedulingPolicyKind::FixedPriority,
        .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{2}},
                        {.task_id = TaskId{2}, .resource_id = ResourceId{1}}}};

    const auto result = build_run_plan(make_run_plan_config(), request);

    REQUIRE(result.valid());
    REQUIRE(result.diagnostics.empty());
    REQUIRE((result.plan->stop_tick() == 0));
    REQUIRE((result.plan->assignments()[0].task_id == TaskId{2}));
    REQUIRE((result.plan->assignments()[1].task_id == TaskId{1}));
}

TEST_CASE("run plan builder rejects incomplete and duplicate assignments", "[run-plan][model]") {
    const RunPlanRequest request{
        .stop_tick = 10,
        .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                        {.task_id = TaskId{1}, .resource_id = ResourceId{2}}}};

    const auto result = build_run_plan(make_run_plan_config(), request);

    REQUIRE_FALSE(result.valid());
    REQUIRE(has_code(result, RunPlanDiagnosticCode::MissingTaskAssignment));
    REQUIRE(has_code(result, RunPlanDiagnosticCode::DuplicateTaskAssignment));
}

TEST_CASE("run plan builder rejects unknown and inaccessible identifiers", "[run-plan][model]") {
    const auto config = make_run_plan_config();

    auto unknown_task = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{2}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{99}, .resource_id = ResourceId{1}}}});
    REQUIRE(has_code(unknown_task, RunPlanDiagnosticCode::UnknownTask));

    auto unknown_resource = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{99}},
                                       {.task_id = TaskId{2}, .resource_id = ResourceId{1}}}});
    REQUIRE(has_code(unknown_resource, RunPlanDiagnosticCode::UnknownResource));

    auto inaccessible = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{2}, .resource_id = ResourceId{2}}}});
    REQUIRE(has_code(inaccessible, RunPlanDiagnosticCode::InaccessibleResource));
}

TEST_CASE("run plan builder validates stop tick and policy boundaries", "[run-plan][model]") {
    const auto assignments =
        std::vector<TaskAssignment>{{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                    {.task_id = TaskId{2}, .resource_id = ResourceId{1}}};

    REQUIRE(build_run_plan(make_run_plan_config(),
                           RunPlanRequest{.stop_tick = 0, .assignments = assignments})
                .valid());
    REQUIRE(build_run_plan(make_run_plan_config(),
                           RunPlanRequest{.stop_tick = 100, .assignments = assignments})
                .valid());

    const auto negative = build_run_plan(
        make_run_plan_config(), RunPlanRequest{.stop_tick = -1, .assignments = assignments});
    REQUIRE(has_code(negative, RunPlanDiagnosticCode::InvalidStopTick));

    const auto unsupported = build_run_plan(
        make_run_plan_config(), RunPlanRequest{.stop_tick = 10,
                                               .policy_kind = static_cast<SchedulingPolicyKind>(99),
                                               .assignments = assignments});
    REQUIRE(has_code(unsupported, RunPlanDiagnosticCode::UnsupportedPolicy));
}

} // namespace
