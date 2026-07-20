/***
 * File: tests/gui/simulation_controller_test.cpp
 * Purpose: Verify the T17 command, snapshot, reset, stepping, and trace-neutral
 *          GUI presentation boundary without opening a graphics window.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/simulation_controller.hpp"

#include "cpssim/functional/mock_functional_model.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/trace/event_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cpssim::ExperimentConfig;
using cpssim::FixedPriorityPolicy;
using cpssim::GuiCommand;
using cpssim::GuiCommandQueue;
using cpssim::GuiExecutionSettings;
using cpssim::GuiFastBatchUnit;
using cpssim::GuiRunMode;
using cpssim::GuiRunState;
using cpssim::MockFunctionalModel;
using cpssim::PeriodicTimingSpec;
using cpssim::PreemptionMode;
using cpssim::ResourceId;
using cpssim::ResourceSpec;
using cpssim::RunPlan;
using cpssim::RunPlanRequest;
using cpssim::SimulationController;
using cpssim::SimulationEngine;
using cpssim::SingleResourceAllocator;
using cpssim::TaskId;
using cpssim::TaskResourceProfile;
using cpssim::TaskSpec;
using cpssim::Tick;

/*** Creates the compact single-resource experiment displayed by these tests. ***/
ExperimentConfig make_gui_test_config() {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        cpssim::SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "cpu"}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 3}}};
}

RunPlan make_gui_test_plan(const ExperimentConfig& config, Tick stop_tick) {
    const auto result = cpssim::build_run_plan(
        config,
        RunPlanRequest{.stop_tick = stop_tick,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}});
    if (!result.valid()) {
        throw std::logic_error{"test run plan must be valid"};
    }
    return *result.plan;
}

/*** Serializes a trace copy for exact GUI/headless comparisons. ***/
std::string serialize_trace(const std::vector<cpssim::Event>& events) {
    std::string result;
    for (const auto& event : events) {
        result += cpssim::serialize_event_json_line(event);
    }
    return result;
}

/*** Verifies that the small command transport preserves insertion order. ***/
TEST_CASE("GUI command queue is FIFO", "[gui][commands]") {
    GuiCommandQueue commands;
    commands.push(GuiCommand::Run);
    commands.push(GuiCommand::Pause);
    commands.push(GuiCommand::Reset);

    REQUIRE((commands.pop() == GuiCommand::Run));
    REQUIRE((commands.pop() == GuiCommand::Pause));
    REQUIRE((commands.pop() == GuiCommand::Reset));
    REQUIRE_FALSE(commands.pop().has_value());
}

/*** Verifies controls, detached snapshots, resource data, and clean reset. ***/
TEST_CASE("GUI controller steps pauses runs and resets through commands",
          "[gui][controller][snapshot]") {
    const auto config = make_gui_test_config();
    SimulationController controller{config, make_gui_test_plan(config, 10)};

    const auto initial = controller.snapshot();
    REQUIRE((initial.run_state == GuiRunState::Paused));
    REQUIRE((initial.current_tick == 0));
    REQUIRE(initial.event_log.empty());
    REQUIRE((initial.resources.size() == 1));

    controller.enqueue(GuiCommand::StepNextEvent);
    controller.update();
    const auto after_first_tick = controller.snapshot();
    REQUIRE((after_first_tick.run_state == GuiRunState::Paused));
    REQUIRE((after_first_tick.current_tick == 0));
    REQUIRE((after_first_tick.event_log.size() == 2));
    REQUIRE(after_first_tick.resources[0].running_job.has_value());

    controller.enqueue(GuiCommand::Run);
    while (controller.snapshot().run_state != GuiRunState::Finished) {
        controller.update();
    }
    const auto finished = controller.snapshot();
    REQUIRE((finished.current_tick == 10));
    REQUIRE((finished.event_log.size() == 5));
    REQUIRE((finished.resources[0].busy_ticks == 3));
    REQUIRE((after_first_tick.event_log.size() == 2));

    controller.enqueue(GuiCommand::Reset);
    controller.update();
    const auto reset = controller.snapshot();
    REQUIRE((reset.run_state == GuiRunState::Paused));
    REQUIRE((reset.current_tick == 0));
    REQUIRE(reset.event_log.empty());
    REQUIRE_FALSE(reset.resources[0].running_job.has_value());
}

/*** Proves that GUI-driven progress does not alter canonical trace bytes. ***/
TEST_CASE("GUI enabled and headless runs produce identical traces", "[gui][determinism][trace]") {
    const auto config = make_gui_test_config();
    const SingleResourceAllocator allocator;

    FixedPriorityPolicy headless_policy;
    SimulationEngine headless{config, 10, allocator, headless_policy};
    headless.run();

    SimulationController controller{config, make_gui_test_plan(config, 10)};
    controller.enqueue(GuiCommand::Run);
    while (controller.snapshot().run_state != GuiRunState::Finished) {
        controller.update();
    }

    REQUIRE(
        (serialize_trace(controller.snapshot().event_log) == serialize_trace(headless.trace())));
}

TEST_CASE("Fast event and tick batches preserve deterministic final output",
          "[gui][controller][fast][determinism]") {
    const auto config = make_gui_test_config();
    SimulationController live{config, make_gui_test_plan(config, 100)};
    SimulationController events{config, make_gui_test_plan(config, 100)};
    SimulationController ticks{config, make_gui_test_plan(config, 100)};
    live.enqueue(GuiCommand::Run);
    events.enqueue(GuiCommand::Run);
    ticks.enqueue(GuiCommand::Run);
    while (live.run_state() != GuiRunState::Finished) live.update();
    const GuiExecutionSettings event_settings{.mode = GuiRunMode::Fast,
                                               .batch_unit = GuiFastBatchUnit::Events,
                                               .event_batch_size = 3,
                                               .tick_batch_size = 7};
    const GuiExecutionSettings tick_settings{.mode = GuiRunMode::Fast,
                                              .batch_unit = GuiFastBatchUnit::Ticks,
                                              .event_batch_size = 3,
                                              .tick_batch_size = 7};
    while (events.run_state() != GuiRunState::Finished) events.update(event_settings);
    while (ticks.run_state() != GuiRunState::Finished) ticks.update(tick_settings);
    REQUIRE(serialize_trace(events.snapshot().event_log) == serialize_trace(live.snapshot().event_log));
    REQUIRE(serialize_trace(ticks.snapshot().event_log) == serialize_trace(live.snapshot().event_log));
    REQUIRE(events.snapshot().resources[0].busy_ticks == live.snapshot().resources[0].busy_ticks);
    REQUIRE(ticks.progress().current_tick == live.progress().current_tick);
}

TEST_CASE("Fast batching validates sizes and exposes lightweight progress",
          "[gui][controller][fast][progress]") {
    const auto config = make_gui_test_config();
    SimulationController controller{config, make_gui_test_plan(config, 100)};
    controller.enqueue(GuiCommand::Run);
    const GuiExecutionSettings settings{.mode = GuiRunMode::Fast,
                                         .batch_unit = GuiFastBatchUnit::Events,
                                         .event_batch_size = 2,
                                         .tick_batch_size = 1000};
    const auto update = controller.update(settings);
    REQUIRE(update.transitions == 2);
    REQUIRE(controller.progress().event_count > 0);
    auto invalid = settings;
    invalid.event_batch_size = 0;
    REQUIRE_THROWS_AS(controller.update(invalid), std::invalid_argument);
}

/*** Verifies G06 copies functional rows and recreates their owner on Reset. ***/
TEST_CASE("GUI snapshots detach functional observations and reset their model",
          "[gui][controller][functional]") {
    const auto config = make_gui_test_config();
    std::size_t models_created = 0;
    const std::vector<cpssim::GuiSignalDescriptor> registry{
        {.id = {cpssim::GuiSignalScalarType::Real, "mock_state"},
         .path = "Functional/Mock/State",
         .display_name = "Mock state",
         .unit = "",
         .source = "mock"}};
    SimulationController controller{config, make_gui_test_plan(config, 10),
                                    [&models_created] {
                                        ++models_created;
                                        return std::make_unique<MockFunctionalModel>(2.0);
                                    },
                                    registry};

    const auto initial = controller.snapshot();
    REQUIRE(initial.functional_model_attached);
    REQUIRE((initial.functional_signal_registry == registry));
    REQUIRE(initial.functional_observations.empty());
    REQUIRE((models_created == 1));

    controller.enqueue(GuiCommand::StepNextEvent);
    controller.update();
    auto retained = controller.snapshot();
    REQUIRE((retained.functional_observations.size() == 1));
    REQUIRE((retained.functional_observations[0].tick == 0));
    REQUIRE((retained.functional_observations[0].real_signals[0].value == 2.0));

    retained.functional_observations[0].real_signals[0].value = -99.0;
    retained.functional_signal_registry[0].display_name = "mutated";
    REQUIRE((controller.snapshot().functional_observations[0].real_signals[0].value == 2.0));
    REQUIRE((controller.snapshot().functional_signal_registry[0].display_name == "Mock state"));

    controller.enqueue(GuiCommand::Run);
    while (controller.snapshot().run_state != GuiRunState::Finished) {
        controller.update();
    }
    REQUIRE((controller.snapshot().functional_observations.size() == 11));

    controller.enqueue(GuiCommand::Reset);
    controller.update();
    const auto reset = controller.snapshot();
    REQUIRE(reset.functional_model_attached);
    REQUIRE(reset.functional_observations.empty());
    REQUIRE((models_created == 2));
}

/*** Proves attaching observation production cannot alter canonical trace bytes. ***/
TEST_CASE("GUI functional observation snapshots leave canonical events unchanged",
          "[gui][controller][functional][determinism]") {
    const auto config = make_gui_test_config();
    SimulationController without_model{config, make_gui_test_plan(config, 10)};
    SimulationController with_model{config, make_gui_test_plan(config, 10),
                                    [] { return std::make_unique<MockFunctionalModel>(); }};

    without_model.enqueue(GuiCommand::Run);
    with_model.enqueue(GuiCommand::Run);
    while (without_model.snapshot().run_state != GuiRunState::Finished) {
        without_model.update();
    }
    while (with_model.snapshot().run_state != GuiRunState::Finished) {
        with_model.update();
    }

    REQUIRE_FALSE(without_model.snapshot().functional_model_attached);
    REQUIRE(without_model.snapshot().functional_observations.empty());
    REQUIRE((serialize_trace(without_model.snapshot().event_log) ==
             serialize_trace(with_model.snapshot().event_log)));
}

} // namespace
