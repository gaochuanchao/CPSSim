/***
 * File: tests/gui/draft_run_plan_test.cpp
 * Purpose: Verify explicit draft editing and atomic GUI run reconstruction.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/functional/mock_functional_model.hpp"
#include "cpssim/gui/simulation_session.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>

namespace {

using namespace cpssim;

ExperimentConfig make_session_config() {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "local"}, ResourceSpec{ResourceId{2}, "cloud"}},
        {TaskSpec{TaskId{1}, "first", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1},
         TaskSpec{TaskId{2}, "second",
                  PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2}},
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

std::string serialize_trace(const std::vector<Event>& events) {
    std::string result;
    for (const auto& event : events) {
        result += serialize_event_json_line(event);
    }
    return result;
}

void complete_draft(GuiSimulationSession& session) {
    REQUIRE(session.set_draft_assignment(TaskId{1}, ResourceId{2}));
    REQUIRE(session.set_draft_assignment(TaskId{2}, ResourceId{1}));
}

RunPlan make_loaded_plan(const ExperimentConfig& config) {
    const auto result = build_run_plan(
        config,
        RunPlanRequest{.stop_tick = 200,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}},
                                       {.task_id = TaskId{2}, .resource_id = ResourceId{1}}}});
    if (!result.valid()) {
        throw std::logic_error{"loaded test plan must be valid"};
    }
    return *result.plan;
}

TEST_CASE("GUI run-plan draft starts explicit and reports missing assignments",
          "[gui][run-plan][draft]") {
    GuiSimulationSession session{make_session_config(), 100};

    REQUIRE_FALSE(session.has_active_run());
    REQUIRE((session.snapshot().run_state == GuiRunState::NotConfigured));
    REQUIRE_FALSE(session.draft().assignment(TaskId{1}).has_value());
    REQUIRE_FALSE(session.draft().assignment(TaskId{2}).has_value());
    REQUIRE(session.draft_dirty());
    REQUIRE(session.last_validation().has_value());
    REQUIRE(has_code(*session.last_validation(), RunPlanDiagnosticCode::MissingTaskAssignment));
}

TEST_CASE("GUI run-plan Apply constructs one clean paused active run", "[gui][run-plan][apply]") {
    GuiSimulationSession session{make_session_config(), 100};
    complete_draft(session);

    REQUIRE(session.validate_draft().valid());
    REQUIRE(session.apply_draft());
    REQUIRE(session.has_active_run());
    REQUIRE_FALSE(session.draft_dirty());
    REQUIRE((session.snapshot().run_state == GuiRunState::Paused));
    REQUIRE((session.snapshot().stop_tick == 100));
    REQUIRE((session.snapshot().experiment.assignments.size() == 2));
    REQUIRE((session.active_plan()->assignments()[0].resource_id == ResourceId{2}));
    REQUIRE((session.active_plan()->assignments()[1].resource_id == ResourceId{1}));
}

TEST_CASE("failed GUI run-plan Apply preserves the active controller", "[gui][run-plan][atomic]") {
    GuiSimulationSession session{make_session_config(), 100};
    complete_draft(session);
    REQUIRE(session.apply_draft());
    REQUIRE(session.enqueue(GuiCommand::StepNextEvent));
    session.update();
    const auto before = session.snapshot();

    REQUIRE(session.set_draft_assignment(TaskId{2}, std::nullopt));
    REQUIRE_FALSE(session.apply_draft());
    const auto after = session.snapshot();

    REQUIRE((after.run_state == before.run_state));
    REQUIRE((after.current_tick == before.current_tick));
    REQUIRE((serialize_trace(after.event_log) == serialize_trace(before.event_log)));
    REQUIRE((session.active_plan()->assignments().size() == 2));
    REQUIRE(has_code(*session.last_validation(), RunPlanDiagnosticCode::MissingTaskAssignment));
}

TEST_CASE("Reset uses the accepted plan and ignores a pending draft", "[gui][run-plan][reset]") {
    GuiSimulationSession session{make_session_config(), 100};
    complete_draft(session);
    REQUIRE(session.apply_draft());
    REQUIRE(session.enqueue(GuiCommand::StepNextEvent));
    session.update();
    REQUIRE_FALSE(session.snapshot().event_log.empty());

    REQUIRE(session.set_draft_stop_tick(200));
    REQUIRE(session.draft_dirty());
    REQUIRE(session.enqueue(GuiCommand::Reset));
    session.update();

    REQUIRE(session.snapshot().event_log.empty());
    REQUIRE((session.snapshot().stop_tick == 100));
    REQUIRE((session.draft().stop_tick() == 200));
    REQUIRE(session.draft_dirty());
}

TEST_CASE("semantic draft editing is disabled while a run is active",
          "[gui][run-plan][lifecycle]") {
    GuiSimulationSession session{make_session_config(), 100};
    complete_draft(session);
    REQUIRE(session.apply_draft());
    REQUIRE(session.enqueue(GuiCommand::Run));
    session.update();

    REQUIRE((session.snapshot().run_state == GuiRunState::Running));
    REQUIRE_FALSE(session.draft_editable());
    REQUIRE_FALSE(session.set_draft_stop_tick(200));
    REQUIRE_FALSE(session.set_draft_assignment(TaskId{1}, ResourceId{1}));
    REQUIRE((session.draft().stop_tick() == 100));
    REQUIRE((session.draft().assignment(TaskId{1}) == ResourceId{2}));
    REQUIRE_FALSE(session.replace_draft(make_loaded_plan(session.config())));
}

TEST_CASE("loaded plans replace only the pending draft", "[gui][run-plan][persistence]") {
    GuiSimulationSession session{make_session_config(), 100};
    complete_draft(session);
    REQUIRE(session.apply_draft());
    REQUIRE(session.enqueue(GuiCommand::StepNextEvent));
    session.update();
    const auto active_before = session.snapshot();

    REQUIRE(session.replace_draft(make_loaded_plan(session.config())));

    const auto active_after = session.snapshot();
    REQUIRE((active_after.stop_tick == active_before.stop_tick));
    REQUIRE((active_after.current_tick == active_before.current_tick));
    REQUIRE((serialize_trace(active_after.event_log) == serialize_trace(active_before.event_log)));
    REQUIRE((session.draft().stop_tick() == 200));
    REQUIRE((session.draft().assignment(TaskId{1}) == ResourceId{1}));
    REQUIRE(session.draft_dirty());
}

TEST_CASE("GUI session applies and resets a recreatable functional source",
          "[gui][run-plan][functional]") {
    std::size_t models_created = 0;
    GuiSimulationSession session{make_session_config(), 100, [&models_created] {
                                     ++models_created;
                                     return std::make_unique<MockFunctionalModel>();
                                 }};
    complete_draft(session);
    REQUIRE(session.apply_draft());
    REQUIRE((models_created == 1));
    REQUIRE(session.snapshot().functional_model_attached);

    REQUIRE(session.enqueue(GuiCommand::StepNextEvent));
    session.update();
    REQUIRE_FALSE(session.snapshot().functional_observations.empty());

    REQUIRE(session.enqueue(GuiCommand::Reset));
    session.update();
    REQUIRE((models_created == 2));
    REQUIRE(session.snapshot().functional_observations.empty());
}

} // namespace
