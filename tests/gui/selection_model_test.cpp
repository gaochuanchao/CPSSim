/***
 * File: tests/gui/selection_model_test.cpp
 * Purpose: Verify G02 strong-identity selection, event matching, and reset
 *          synchronization without opening a graphics window.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/selection_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace {

using namespace cpssim;

/*** Creates a compact run used to exercise runtime-only selection invalidation. ***/
ExperimentConfig make_selection_config() {
    return ExperimentConfig{
        std::chrono::microseconds{100},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{4}, "cpu"}},
        {TaskSpec{TaskId{7}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{7}, .resource_id = ResourceId{4}, .execution_time = 3}}};
}

/*** Verifies every selection domain retains its strong typed identity. ***/
TEST_CASE("GUI selection stores strong entity identities", "[gui][selection]") {
    GuiSelection selection;
    REQUIRE((selection.kind() == GuiSelectionKind::None));

    selection.select_experiment();
    REQUIRE((selection.kind() == GuiSelectionKind::Experiment));

    selection.select_task(TaskId{7});
    REQUIRE((selection.kind() == GuiSelectionKind::Task));
    REQUIRE((selection.task_id() == TaskId{7}));

    selection.select_resource(ResourceId{4});
    REQUIRE((selection.kind() == GuiSelectionKind::Resource));
    REQUIRE((selection.resource_id() == ResourceId{4}));

    const GuiRouteIdentity route{TaskId{7}, TaskId{8}};
    selection.select_route(route);
    REQUIRE((selection.kind() == GuiSelectionKind::Route));
    REQUIRE((selection.route_id() == route));

    const JobIdentity job{TaskId{7}, JobId{3}};
    selection.select_job(job);
    REQUIRE((selection.kind() == GuiSelectionKind::Job));
    REQUIRE((selection.job() == job));

    selection.select_event(EventSequence{12});
    REQUIRE((selection.kind() == GuiSelectionKind::Event));
    REQUIRE((selection.event_sequence() == EventSequence{12}));

    selection.select_tick_range({.begin_tick = 9, .end_tick = 4});
    REQUIRE((selection.tick_range() == GuiTickRange{.begin_tick = 4, .end_tick = 9}));
    REQUIRE((selection.kind() == GuiSelectionKind::Event));
    selection.select_tick(5);
    REQUIRE((selection.tick_range() == GuiTickRange{.begin_tick = 5, .end_tick = 5}));

    selection.clear();
    REQUIRE((selection.kind() == GuiSelectionKind::None));
    REQUIRE_FALSE(selection.tick_range().has_value());
}

TEST_CASE("structural and runtime selections retain independent strong identities",
          "[gui][selection][structural]") {
    StructuralSelection structural;
    GuiSelection runtime;

    structural.select_task(TaskId{7});
    runtime.select_event(EventSequence{12});
    REQUIRE((structural.kind() == StructuralSelectionKind::Task));
    REQUIRE((structural.task_id() == TaskId{7}));
    REQUIRE((runtime.kind() == GuiSelectionKind::Event));

    runtime.select_resource(ResourceId{4});
    REQUIRE((structural.task_id() == TaskId{7}));
    structural.select_execution_profile({TaskId{1}, ResourceId{1}});
    REQUIRE((runtime.resource_id() == ResourceId{4}));

    const auto draft = EditableSystemDraft::minimal();
    synchronize_structural_selection(structural, draft);
    REQUIRE((structural.kind() == StructuralSelectionKind::ExecutionProfile));

    structural.select_connection(
        {GuiConnectionKind::Logical, TaskId{1}, TaskId{2}});
    REQUIRE(structural.kind() == StructuralSelectionKind::Connection);
    REQUIRE(runtime.resource_id() == ResourceId{4});

    synchronize_structural_selection(structural, draft);
    REQUIRE((structural.kind() == StructuralSelectionKind::System));
}

/*** Verifies event emphasis uses stable typed references, never labels/indexes. ***/
TEST_CASE("GUI selection matches related canonical events", "[gui][selection][event]") {
    const Event event{5,
                      EventPhase::Scheduling,
                      EventSequence{12},
                      EventType::JobStart,
                      {.task_id = TaskId{7},
                       .job_id = JobId{3},
                       .resource_id = ResourceId{4},
                       .message_id = std::nullopt,
                       .vehicle_id = std::nullopt}};
    GuiSelection selection;

    selection.select_task(TaskId{7});
    REQUIRE(event_matches_selection(event, selection));
    selection.select_resource(ResourceId{4});
    REQUIRE(event_matches_selection(event, selection));
    selection.select_job(JobIdentity{TaskId{7}, JobId{3}});
    REQUIRE(event_matches_selection(event, selection));
    selection.select_event(EventSequence{12});
    REQUIRE(event_matches_selection(event, selection));

    selection.select_task(TaskId{8});
    selection.select_tick(5);
    REQUIRE(event_matches_selection(event, selection));
    selection.select_tick(6);
    REQUIRE_FALSE(event_matches_selection(event, selection));

    selection.clear_tick_range();
    REQUIRE_FALSE(event_matches_selection(event, selection));
}

/*** Keeps immutable selections and clears event/job identities after reset. ***/
TEST_CASE("GUI selection synchronizes runtime identities across reset", "[gui][selection][reset]") {
    const auto config = make_selection_config();
    const auto plan = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{7}, .resource_id = ResourceId{4}}}});
    REQUIRE(plan.valid());
    SimulationController controller{config, *plan.plan};
    controller.enqueue(GuiCommand::StepNextEvent);
    controller.update();
    const auto active = controller.snapshot();
    REQUIRE_FALSE(active.event_log.empty());
    REQUIRE(active.resources[0].running_job.has_value());

    GuiSelection selection;
    selection.select_event(active.event_log.front().sequence());
    selection.select_tick(active.event_log.front().tick());
    synchronize_selection(selection, active);
    REQUIRE((selection.kind() == GuiSelectionKind::Event));

    controller.enqueue(GuiCommand::Reset);
    controller.update();
    const auto reset = controller.snapshot();
    REQUIRE((reset.experiment == active.experiment));
    synchronize_selection(selection, reset);
    REQUIRE((selection.kind() == GuiSelectionKind::None));
    REQUIRE_FALSE(selection.tick_range().has_value());

    selection.select_job(*active.resources[0].running_job);
    synchronize_selection(selection, reset);
    REQUIRE((selection.kind() == GuiSelectionKind::None));

    selection.select_task(TaskId{7});
    synchronize_selection(selection, reset);
    REQUIRE((selection.kind() == GuiSelectionKind::Task));

    selection.select_resource(ResourceId{999});
    synchronize_selection(selection, reset);
    REQUIRE((selection.kind() == GuiSelectionKind::None));
}

} // namespace
