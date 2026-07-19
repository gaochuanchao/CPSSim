/***
 * File: tests/gui/timeline_model_test.cpp
 * Purpose: Verify strict G05a timeline lifecycle derivation and diagnostics.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/simulation_controller.hpp"
#include "cpssim/gui/timeline_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace cpssim;

ExperimentPresentationSnapshot make_timeline_experiment(bool reverse = false) {
    std::vector<ResourceSpec> resources;
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    if (reverse) {
        resources.emplace_back(ResourceId{2}, "cloud");
        resources.emplace_back(ResourceId{1}, "local");
        tasks.emplace_back(TaskId{2}, "high",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 1);
        tasks.emplace_back(TaskId{1}, "low",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2);
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{2}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 8});
    } else {
        resources.emplace_back(ResourceId{1}, "local");
        resources.emplace_back(ResourceId{2}, "cloud");
        tasks.emplace_back(TaskId{1}, "low",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2);
        tasks.emplace_back(TaskId{2}, "high",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 1);
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 8});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{1}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{2}, .execution_time = 2});
    }
    return build_experiment_presentation(
        ExperimentConfig{std::chrono::microseconds{100},
                         SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                         std::move(resources), std::move(tasks), std::move(profiles)});
}

Event job_event(Tick tick, std::uint64_t sequence, EventPhase phase, EventType type,
                std::uint64_t task, std::uint64_t job, std::uint64_t resource) {
    return Event{tick,
                 phase,
                 EventSequence{sequence},
                 type,
                 {.task_id = TaskId{task},
                  .job_id = JobId{job},
                  .resource_id = ResourceId{resource},
                  .message_id = std::nullopt,
                  .vehicle_id = std::nullopt}};
}

const GuiTimelineRow& row(const GuiTimelineModel& timeline, ResourceId resource_id) {
    for (const auto& candidate : timeline.rows) {
        if (candidate.resource_id == resource_id) {
            return candidate;
        }
    }
    throw std::logic_error{"timeline test row is unavailable"};
}

TEST_CASE("timeline derives Ready and first running intervals", "[gui][timeline][build]") {
    const std::vector<Event> events{
        job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
        job_event(2, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
        job_event(5, 1, EventPhase::ExecutionCompletion, EventType::JobFinish, 1, 1, 1),
    };
    const auto result = build_timeline_model(events, make_timeline_experiment(), 5);

    REQUIRE(result.valid());
    REQUIRE((result.timeline->rows.size() == 2));
    REQUIRE((row(*result.timeline, ResourceId{1}).intervals.size() == 2));
    REQUIRE((row(*result.timeline, ResourceId{2}).intervals.empty()));
    const auto& ready = row(*result.timeline, ResourceId{1}).intervals[0];
    const auto& running = row(*result.timeline, ResourceId{1}).intervals[1];
    REQUIRE((ready.kind == GuiTimelineIntervalKind::Ready));
    REQUIRE((ready.begin_tick == 0));
    REQUIRE((ready.end_tick == 2));
    REQUIRE((ready.begin_sequence == EventSequence{0}));
    REQUIRE((ready.end_sequence == EventSequence{2}));
    REQUIRE((running.kind == GuiTimelineIntervalKind::Running));
    REQUIRE((running.begin_tick == 2));
    REQUIRE((running.end_tick == 5));
    REQUIRE_FALSE(running.resumed);
    REQUIRE((result.timeline->markers.size() == events.size()));
}

TEST_CASE("timeline derives preemption Ready and resumed execution intervals",
          "[gui][timeline][preemption]") {
    const std::vector<Event> events{
        job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
        job_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
        job_event(4, 4, EventPhase::Scheduling, EventType::JobPreempt, 1, 1, 1),
        job_event(7, 5, EventPhase::Scheduling, EventType::JobResume, 1, 1, 1),
        job_event(9, 3, EventPhase::ExecutionCompletion, EventType::JobFinish, 1, 1, 1),
    };
    const auto result = build_timeline_model(events, make_timeline_experiment(), 9);

    REQUIRE(result.valid());
    const auto& intervals = row(*result.timeline, ResourceId{1}).intervals;
    REQUIRE((intervals.size() == 4));
    REQUIRE((intervals[0].kind == GuiTimelineIntervalKind::Ready));
    REQUIRE((intervals[0].begin_tick == 0));
    REQUIRE((intervals[0].end_tick == 0));
    REQUIRE((intervals[1].kind == GuiTimelineIntervalKind::Running));
    REQUIRE((intervals[1].begin_tick == 0));
    REQUIRE((intervals[1].end_tick == 4));
    REQUIRE((intervals[2].kind == GuiTimelineIntervalKind::Ready));
    REQUIRE((intervals[2].begin_tick == 4));
    REQUIRE((intervals[2].end_tick == 7));
    REQUIRE((intervals[3].kind == GuiTimelineIntervalKind::Running));
    REQUIRE((intervals[3].begin_tick == 7));
    REQUIRE((intervals[3].end_tick == 9));
    REQUIRE(intervals[3].resumed);
}

TEST_CASE("timeline keeps unfinished Ready and Running intervals explicitly open",
          "[gui][timeline][live]") {
    const std::vector<Event> events{
        job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
        job_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
        job_event(1, 3, EventPhase::JobRelease, EventType::JobRelease, 2, 1, 2),
    };
    const auto result = build_timeline_model(events, make_timeline_experiment(), 5);

    REQUIRE(result.valid());
    const auto& running = row(*result.timeline, ResourceId{1}).intervals.back();
    const auto& ready = row(*result.timeline, ResourceId{2}).intervals.back();
    REQUIRE((running.kind == GuiTimelineIntervalKind::Running));
    REQUIRE_FALSE(running.end_tick.has_value());
    REQUIRE_FALSE(running.end_sequence.has_value());
    REQUIRE((ready.kind == GuiTimelineIntervalKind::Ready));
    REQUIRE_FALSE(ready.end_tick.has_value());
    REQUIRE((result.timeline->current_tick == 5));
}

TEST_CASE("timeline preserves deadline and message marker identities", "[gui][timeline][marker]") {
    const std::vector<Event> events{
        job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
        job_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
        job_event(3, 1, EventPhase::DeadlineCheck, EventType::DeadlineMiss, 1, 1, 1),
        job_event(4, 3, EventPhase::ExecutionCompletion, EventType::JobFinish, 1, 1, 1),
        Event{5,
              EventPhase::CausedAction,
              EventSequence{4},
              EventType::MessageSend,
              {.task_id = TaskId{1},
               .job_id = JobId{1},
               .resource_id = std::nullopt,
               .message_id = MessageId{7},
               .vehicle_id = std::nullopt},
              EventSequence{3}},
        Event{8,
              EventPhase::MessageDelivery,
              EventSequence{5},
              EventType::MessageDelivery,
              {.task_id = TaskId{2},
               .job_id = std::nullopt,
               .resource_id = std::nullopt,
               .message_id = MessageId{7},
               .vehicle_id = std::nullopt},
              EventSequence{4}},
    };
    const auto result = build_timeline_model(events, make_timeline_experiment(), 8);

    REQUIRE(result.valid());
    REQUIRE((result.timeline->markers.size() == 6));
    REQUIRE((result.timeline->markers[2].type == EventType::DeadlineMiss));
    REQUIRE((result.timeline->markers[4].message_id == MessageId{7}));
    REQUIRE((result.timeline->markers[5].type == EventType::MessageDelivery));
    const auto* marker = find_timeline_marker(*result.timeline, EventSequence{4});
    REQUIRE((marker != nullptr));
    REQUIRE((marker->type == EventType::MessageSend));
    REQUIRE((marker->tick == 5));
    REQUIRE((find_timeline_marker(*result.timeline, EventSequence{99}) == nullptr));
}

TEST_CASE("timeline derives independent resource intervals deterministically",
          "[gui][timeline][determinism]") {
    const std::vector<Event> events{
        job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
        job_event(0, 1, EventPhase::JobRelease, EventType::JobRelease, 2, 1, 2),
        job_event(0, 3, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
        job_event(0, 4, EventPhase::Scheduling, EventType::JobStart, 2, 1, 2),
        job_event(2, 2, EventPhase::ExecutionCompletion, EventType::JobFinish, 2, 1, 2),
        job_event(5, 5, EventPhase::ExecutionCompletion, EventType::JobFinish, 1, 1, 1),
    };
    const auto forward = build_timeline_model(events, make_timeline_experiment(), 5);
    const auto reverse = build_timeline_model(events, make_timeline_experiment(true), 5);

    REQUIRE(forward.valid());
    REQUIRE(reverse.valid());
    REQUIRE((forward.timeline == reverse.timeline));
    REQUIRE((row(*forward.timeline, ResourceId{1}).intervals.size() == 2));
    REQUIRE((row(*forward.timeline, ResourceId{2}).intervals.size() == 2));
}

TEST_CASE("timeline accepts a complete controller trace and rebuilds empty after reset",
          "[gui][timeline][controller]") {
    const ExperimentConfig config{
        std::chrono::microseconds{100},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 3}}};
    const auto plan = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}});
    REQUIRE(plan.valid());
    SimulationController controller{config, *plan.plan};
    controller.enqueue(GuiCommand::Run);
    while (controller.snapshot().run_state != GuiRunState::Finished) {
        controller.update();
    }

    const auto finished = controller.snapshot();
    const auto timeline =
        build_timeline_model(finished.event_log, finished.experiment, finished.current_tick);
    REQUIRE(timeline.valid());
    REQUIRE((timeline.timeline->markers.size() == finished.event_log.size()));
    REQUIRE((row(*timeline.timeline, ResourceId{1}).intervals.size() == 4));
    REQUIRE_FALSE(row(*timeline.timeline, ResourceId{1}).intervals.back().end_tick.has_value());

    controller.enqueue(GuiCommand::Reset);
    controller.update();
    const auto reset = controller.snapshot();
    const auto empty = build_timeline_model(reset.event_log, reset.experiment, reset.current_tick);
    REQUIRE(empty.valid());
    REQUIRE(empty.timeline->markers.empty());
    REQUIRE(row(*empty.timeline, ResourceId{1}).intervals.empty());
}

TEST_CASE("timeline cache incrementally equals a full rebuild and handles reset",
          "[gui][timeline][cache]") {
    const ExperimentConfig config{
        std::chrono::microseconds{100},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 3}}};
    const auto plan = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 10,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}});
    REQUIRE(plan.valid());
    SimulationController controller{config, *plan.plan};
    GuiTimelineCache cache;

    auto snapshot = controller.snapshot();
    auto expected =
        build_timeline_model(snapshot.event_log, snapshot.experiment, snapshot.current_tick);
    REQUIRE(
        (cache.update(snapshot.event_log, snapshot.experiment, snapshot.current_tick).timeline ==
         expected.timeline));

    controller.enqueue(GuiCommand::Run);
    while (snapshot.run_state != GuiRunState::Finished) {
        controller.update();
        snapshot = controller.snapshot();
        expected =
            build_timeline_model(snapshot.event_log, snapshot.experiment, snapshot.current_tick);
        const auto& incremental =
            cache.update(snapshot.event_log, snapshot.experiment, snapshot.current_tick);
        REQUIRE(incremental.valid());
        REQUIRE((incremental.timeline == expected.timeline));
    }

    controller.enqueue(GuiCommand::Reset);
    controller.update();
    snapshot = controller.snapshot();
    const auto& reset =
        cache.update(snapshot.event_log, snapshot.experiment, snapshot.current_tick);
    REQUIRE(reset.valid());
    REQUIRE(reset.timeline->markers.empty());
    REQUIRE(row(*reset.timeline, ResourceId{1}).intervals.empty());
}

TEST_CASE("timeline cache reports an invalid suffix without corrupting its prefix",
          "[gui][timeline][cache][invalid]") {
    const auto experiment = make_timeline_experiment();
    const auto release = job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1);
    GuiTimelineCache cache;
    REQUIRE(cache.update({release}, experiment, 0).valid());

    const auto bad_resume = job_event(1, 1, EventPhase::Scheduling, EventType::JobResume, 1, 1, 1);
    const auto& invalid = cache.update({release, bad_resume}, experiment, 1);
    REQUIRE_FALSE(invalid.valid());
    REQUIRE((invalid.diagnostics[0].event_index == 1));
    REQUIRE((invalid.diagnostics[0].message.find("event[1] sequence 1 at tick 1") !=
             std::string::npos));

    const auto start = job_event(1, 1, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1);
    const std::vector<Event> corrected{release, start};
    const auto& recovered = cache.update(corrected, experiment, 2);
    const auto rebuilt = build_timeline_model(corrected, experiment, 2);
    REQUIRE(recovered.valid());
    REQUIRE((recovered.timeline == rebuilt.timeline));
}

TEST_CASE("timeline diagnostics identify the exact invalid event", "[gui][timeline][invalid]") {
    const auto experiment = make_timeline_experiment();

    SECTION("start without release") {
        const auto result = build_timeline_model(
            {job_event(2, 8, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1)}, experiment, 2);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics.size() == 1));
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::InvalidJobTransition));
        REQUIRE((result.diagnostics[0].event_index == 0));
        REQUIRE((result.diagnostics[0].event_sequence == EventSequence{8}));
        REQUIRE((result.diagnostics[0].message.find("event[0] sequence 8") != std::string::npos));
        REQUIRE((result.diagnostics[0].message.find("T1:J1") != std::string::npos));
    }

    SECTION("missing resource") {
        const Event release{0,
                            EventPhase::JobRelease,
                            EventSequence{4},
                            EventType::JobRelease,
                            {.task_id = TaskId{1},
                             .job_id = JobId{1},
                             .resource_id = std::nullopt,
                             .message_id = std::nullopt,
                             .vehicle_id = std::nullopt}};
        const auto result = build_timeline_model({release}, experiment, 0);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::MissingEntity));
        REQUIRE((result.diagnostics[0].message.find("resource_id") != std::string::npos));
    }

    SECTION("overlapping resource execution") {
        const std::vector<Event> events{
            job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
            job_event(0, 1, EventPhase::JobRelease, EventType::JobRelease, 2, 1, 1),
            job_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 1),
            job_event(0, 3, EventPhase::Scheduling, EventType::JobStart, 2, 1, 1),
        };
        const auto result = build_timeline_model(events, experiment, 0);
        REQUIRE_FALSE(result.valid());
        REQUIRE(
            (result.diagnostics[0].code == GuiTimelineDiagnosticCode::OverlappingRunningIntervals));
        REQUIRE((result.diagnostics[0].event_index == 3));
        REQUIRE((result.diagnostics[0].message.find("R1") != std::string::npos));
    }

    SECTION("resource mismatch") {
        const std::vector<Event> events{
            job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
            job_event(0, 2, EventPhase::Scheduling, EventType::JobStart, 1, 1, 2),
        };
        const auto result = build_timeline_model(events, experiment, 0);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::ResourceMismatch));
        REQUIRE((result.diagnostics[0].event_index == 1));
        REQUIRE((result.diagnostics[0].message.find("expected R1, found R2") != std::string::npos));
    }

    SECTION("resume without preemption") {
        const std::vector<Event> events{
            job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
            job_event(1, 2, EventPhase::Scheduling, EventType::JobResume, 1, 1, 1),
        };
        const auto result = build_timeline_model(events, experiment, 1);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::InvalidJobTransition));
        REQUIRE((result.diagnostics[0].event_index == 1));
        REQUIRE((result.diagnostics[0].message.find("previously preempted") != std::string::npos));
    }

    SECTION("finish without running") {
        const std::vector<Event> events{
            job_event(0, 0, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
            job_event(2, 1, EventPhase::ExecutionCompletion, EventType::JobFinish, 1, 1, 1),
        };
        const auto result = build_timeline_model(events, experiment, 2);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::InvalidJobTransition));
        REQUIRE((result.diagnostics[0].event_index == 1));
        REQUIRE(
            (result.diagnostics[0].message.find("requires Running T1:J1") != std::string::npos));
    }

    SECTION("duplicate event sequence") {
        const std::vector<Event> events{
            job_event(0, 4, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1),
            job_event(0, 4, EventPhase::JobRelease, EventType::JobRelease, 2, 1, 2),
        };
        const auto result = build_timeline_model(events, experiment, 0);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::DuplicateEventSequence));
        REQUIRE((result.diagnostics[0].event_index == 1));
    }

    SECTION("same-tick phase order") {
        const auto release =
            job_event(3, 10, EventPhase::JobRelease, EventType::JobRelease, 1, 1, 1);
        const Event delivery{3,
                             EventPhase::MessageDelivery,
                             EventSequence{11},
                             EventType::MessageDelivery,
                             {.task_id = TaskId{2},
                              .job_id = std::nullopt,
                              .resource_id = std::nullopt,
                              .message_id = MessageId{1},
                              .vehicle_id = std::nullopt}};
        const auto result = build_timeline_model({release, delivery}, experiment, 3);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiTimelineDiagnosticCode::InvalidTraceOrder));
        REQUIRE((result.diagnostics[0].event_index == 1));
        REQUIRE((result.diagnostics[0].message.find("message_delivery") != std::string::npos));
    }
}

} // namespace
