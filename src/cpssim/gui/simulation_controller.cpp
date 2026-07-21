/***
 * File: src/cpssim/gui/simulation_controller.cpp
 * Purpose: Implement queued GUI controls, clean runtime reconstruction, and
 *          detached resource/event snapshots.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: All methods run on the GUI application's single thread. The
 *        command boundary can later be synchronized without changing kernel
 *        semantics or exposing mutable state.
 ***/

#include "cpssim/gui/simulation_controller.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cpssim {
namespace {

/*** Revalidates a typed plan when it is paired with a controller experiment. ***/
RunPlan validate_controller_plan(const ExperimentConfig& config, const RunPlan& plan) {
    const auto result = build_run_plan(config, RunPlanRequest{.stop_tick = plan.stop_tick(),
                                                              .policy_kind = plan.policy_kind(),
                                                              .assignments = plan.assignments()});
    if (!result.plan.has_value()) {
        if (result.diagnostics.empty()) {
            throw std::invalid_argument{"run-plan validation failed without a diagnostic"};
        }
        throw std::invalid_argument{result.diagnostics.front().message};
    }
    if (!result.diagnostics.empty()) {
        throw std::invalid_argument{result.diagnostics.front().message};
    }
    return result.plan.value();
}

} // namespace

/*** Adds a command to the back of the single-threaded FIFO. ***/
void GuiCommandQueue::push(GuiCommand command) { commands_.push_back(command); }

/*** Removes the front command while representing an empty queue explicitly. ***/
std::optional<GuiCommand> GuiCommandQueue::pop() {
    if (commands_.empty()) {
        return std::nullopt;
    }
    const auto command = commands_.front();
    commands_.pop_front();
    return command;
}

/*** Captures input and allocation, then builds the first clean runtime. ***/
SimulationController::SimulationController(
    ExperimentConfig config, const RunPlan& run_plan,
    GuiFunctionalModelFactory functional_model_factory,
    std::vector<GuiSignalDescriptor> functional_signal_registry)
    : config_{std::move(config)}, run_plan_{validate_controller_plan(config_, run_plan)},
      allocator_{run_plan_.assignments()},
      functional_model_factory_{std::move(functional_model_factory)},
      functional_signal_registry_{std::move(functional_signal_registry)} {
    reset();
    experiment_presentation_ = build_experiment_presentation(config_, run_plan_.assignments());
}

/*** Appends a control request for the next safe controller update. ***/
void SimulationController::enqueue(GuiCommand command) { commands_.push(command); }

/*** Builds a complete replacement before releasing the current ownership graph. ***/
void SimulationController::reset() {
    std::unique_ptr<FixedPriorityPolicy> replacement_policy;
    switch (run_plan_.policy_kind()) {
    case SchedulingPolicyKind::FixedPriority:
        replacement_policy = std::make_unique<FixedPriorityPolicy>();
        break;
    }
    std::unique_ptr<FunctionalModel> replacement_model;
    std::unique_ptr<SimulationEngine> replacement_engine;
    if (functional_model_factory_) {
        replacement_model = functional_model_factory_();
        if (replacement_model == nullptr) {
            throw std::runtime_error{"GUI functional-model factory returned no model"};
        }
        replacement_engine = std::make_unique<SimulationEngine>(
            config_, run_plan_.stop_tick(), allocator_, *replacement_policy, *replacement_model);
    } else {
        replacement_engine = std::make_unique<SimulationEngine>(config_, run_plan_.stop_tick(),
                                                                allocator_, *replacement_policy);
    }

    engine_.reset();
    functional_model_ = std::move(replacement_model);
    policy_ = std::move(replacement_policy);
    engine_ = std::move(replacement_engine);
    run_state_ = GuiRunState::Paused;
    ++simulation_data_generation_;
}

/*** Delegates one logical-tick transition and reflects terminal state. ***/
void SimulationController::step_once() {
    engine_->step_to_next_event();
    ++simulation_data_generation_;
    if (engine_->finished()) {
        run_state_ = GuiRunState::Finished;
    }
}

/*** Applies queued controls in insertion order and advances a running session. ***/
GuiControllerUpdateResult SimulationController::update(const GuiExecutionSettings& settings) {
    if (settings.event_batch_size == 0 || settings.tick_batch_size == 0) {
        throw std::invalid_argument{"GUI execution batch sizes must be positive"};
    }
    GuiControllerUpdateResult result;
    const auto state_before = run_state_;
    while (const auto command = commands_.pop()) {
        result.command_processed = true;
        switch (*command) {
        case GuiCommand::Run:
            if (!engine_->finished()) {
                run_state_ = GuiRunState::Running;
            }
            break;
        case GuiCommand::Pause:
            if (!engine_->finished()) {
                run_state_ = GuiRunState::Paused;
                result.paused = true;
            }
            break;
        case GuiCommand::Reset:
            reset();
            running_wall_time_ = {};
            result.reset = true;
            break;
        case GuiCommand::StepNextEvent:
            if (!engine_->finished()) {
                run_state_ = GuiRunState::Paused;
                step_once();
                result.step_completed = true;
                ++result.transitions;
            }
            break;
        }
    }

    last_execution_settings_ = settings;
    if (run_state_ == GuiRunState::Running) {
        const auto started = std::chrono::steady_clock::now();
        if (settings.mode == GuiRunMode::Live) {
            step_once();
            result.transitions = 1;
        } else {
            const auto start_tick = engine_->current_tick();
            const auto remaining = std::numeric_limits<Tick>::max() - start_tick;
            const auto tick_budget = static_cast<Tick>(std::min<std::uint64_t>(
                settings.tick_batch_size, static_cast<std::uint64_t>(remaining)));
            const auto target_tick = start_tick + tick_budget;
            while (run_state_ == GuiRunState::Running) {
                if (settings.batch_unit == GuiFastBatchUnit::Events &&
                    result.transitions >= settings.event_batch_size) {
                    break;
                }
                if (settings.batch_unit == GuiFastBatchUnit::Ticks && result.transitions > 0 &&
                    engine_->current_tick() >= target_tick) {
                    break;
                }
                if (std::chrono::steady_clock::now() - started >= settings.wall_clock_budget) {
                    break;
                }
                step_once();
                ++result.transitions;
            }
        }
        running_wall_time_ += std::chrono::steady_clock::now() - started;
    }
    result.finished = state_before != GuiRunState::Finished && run_state_ == GuiRunState::Finished;
    return result;
}

SimulationProgress SimulationController::progress() const {
    return {.run_state = run_state_,
            .current_tick = engine_->current_tick(),
            .stop_tick = run_plan_.stop_tick(),
            .event_count = engine_->trace().size()};
}

RunPerformanceSummary SimulationController::performance_summary() const {
    const auto seconds = std::chrono::duration<double>(running_wall_time_).count();
    const auto progress_value = progress();
    const auto batch_size = last_execution_settings_.batch_unit == GuiFastBatchUnit::Events
                                ? last_execution_settings_.event_batch_size
                                : last_execution_settings_.tick_batch_size;
    return {.wall_clock_duration = running_wall_time_,
            .events_per_second =
                seconds > 0.0 ? static_cast<double>(progress_value.event_count) / seconds : 0.0,
            .ticks_per_second =
                seconds > 0.0 ? static_cast<double>(progress_value.current_tick) / seconds : 0.0,
            .mode = last_execution_settings_.mode,
            .batch_unit = last_execution_settings_.batch_unit,
            .batch_size = batch_size};
}

/*** Copies experiment, trace, and public runtime views into presentation data. ***/
SimulationSnapshot SimulationController::snapshot() const {
    std::vector<GuiResourceSnapshot> resources;
    resources.reserve(experiment_presentation_.resources.size());
    for (const auto& resource_spec : experiment_presentation_.resources) {
        const auto resource_id = resource_spec.id;
        const auto& resource = engine_->scheduler().resource(resource_id);
        resources.push_back({.id = resource_id,
                             .name = resource_spec.name,
                             .running_job = resource.running_job(),
                             .ready_jobs = engine_->scheduler().ready_jobs(resource_id),
                             .busy_ticks = resource.busy_ticks_until(engine_->current_tick()),
                             .idle_ticks = resource.idle_ticks_until(engine_->current_tick())});
    }

    return SimulationSnapshot{.run_state = run_state_,
                              .current_tick = engine_->current_tick(),
                              .stop_tick = run_plan_.stop_tick(),
                              .experiment = experiment_presentation_,
                              .event_log = engine_->trace(),
                              .functional_model_attached = functional_model_ != nullptr,
                              .functional_signal_registry = functional_signal_registry_,
                              .functional_observations = engine_->functional_trace(),
                              .resources = std::move(resources)};
}

} // namespace cpssim
