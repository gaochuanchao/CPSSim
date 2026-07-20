/***
 * File: src/cpssim/gui/simulation_session.cpp
 * Purpose: Implement draft ownership and strong-guarantee controller Apply.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/simulation_session.hpp"

#include <exception>
#include <utility>

namespace cpssim {

GuiSimulationSession::GuiSimulationSession(
    ExperimentConfig config, Tick initial_stop_tick,
    GuiFunctionalModelFactory functional_model_factory,
    std::vector<GuiSignalDescriptor> functional_signal_registry)
    : config_{std::move(config)}, functional_model_factory_{std::move(functional_model_factory)},
      functional_signal_registry_{std::move(functional_signal_registry)},
      experiment_presentation_{build_experiment_presentation(config_)},
      draft_{config_, initial_stop_tick} {
    validate_draft();
}

const RunPlan* GuiSimulationSession::active_plan() const {
    return controller_ != nullptr ? &controller_->run_plan() : nullptr;
}

bool GuiSimulationSession::draft_editable() const {
    return controller_ == nullptr || controller_->run_state() != GuiRunState::Running;
}

bool GuiSimulationSession::draft_dirty() const { return draft_.dirty(config_, active_plan()); }

void GuiSimulationSession::draft_changed() { last_validation_.reset(); }

bool GuiSimulationSession::set_draft_stop_tick(Tick stop_tick) {
    if (!draft_editable()) {
        return false;
    }
    draft_.set_stop_tick(stop_tick);
    draft_changed();
    return true;
}

bool GuiSimulationSession::set_draft_policy_kind(SchedulingPolicyKind policy_kind) {
    if (!draft_editable()) {
        return false;
    }
    draft_.set_policy_kind(policy_kind);
    draft_changed();
    return true;
}

bool GuiSimulationSession::set_draft_assignment(TaskId task_id,
                                                std::optional<ResourceId> resource_id) {
    if (!draft_editable()) {
        return false;
    }
    draft_.set_assignment(task_id, resource_id);
    draft_changed();
    return true;
}

bool GuiSimulationSession::replace_draft(const RunPlan& plan) {
    if (!draft_editable()) {
        return false;
    }

    RunPlanRequest request{.stop_tick = plan.stop_tick(),
                           .policy_kind = plan.policy_kind(),
                           .assignments = plan.assignments()};
    auto validation = build_run_plan(config_, request);
    if (!validation.valid()) {
        last_validation_ = std::move(validation);
        return false;
    }

    RunPlanDraft replacement{config_, plan.stop_tick()};
    replacement.set_policy_kind(plan.policy_kind());
    for (const auto& assignment : plan.assignments()) {
        replacement.set_assignment(assignment.task_id, assignment.resource_id);
    }
    draft_ = std::move(replacement);
    last_validation_ = std::move(validation);
    return true;
}

const RunPlanBuildResult& GuiSimulationSession::validate_draft() {
    last_validation_ = draft_.build(config_);
    return *last_validation_;
}

bool GuiSimulationSession::apply_draft() {
    if (!draft_editable()) {
        return false;
    }

    const auto& validation = validate_draft();
    if (!validation.plan.has_value() || !validation.diagnostics.empty()) {
        return false;
    }

    try {
        auto replacement = std::make_unique<SimulationController>(config_, validation.plan.value(),
                                                                  functional_model_factory_,
                                                                  functional_signal_registry_);
        controller_ = std::move(replacement);
        ++run_generation_;
        return true;
    } catch (const std::exception& error) {
        last_validation_->plan.reset();
        last_validation_->diagnostics.push_back(
            {.code = RunPlanDiagnosticCode::RunConstructionFailed,
             .task_id = std::nullopt,
             .resource_id = std::nullopt,
             .message = error.what()});
        return false;
    }
}

bool GuiSimulationSession::enqueue(GuiCommand command) {
    if (controller_ == nullptr) {
        return false;
    }
    controller_->enqueue(command);
    return true;
}

GuiControllerUpdateResult GuiSimulationSession::update(const GuiExecutionSettings& settings) {
    if (controller_ != nullptr) {
        auto result = controller_->update(settings);
        if (result.reset) {
            ++run_generation_;
        }
        return result;
    }
    return {};
}

SimulationProgress GuiSimulationSession::progress() const {
    return controller_ != nullptr
               ? controller_->progress()
               : SimulationProgress{GuiRunState::NotConfigured, 0, draft_.stop_tick(), 0};
}

RunPerformanceSummary GuiSimulationSession::performance_summary() const {
    return controller_ != nullptr ? controller_->performance_summary() : RunPerformanceSummary{};
}

SimulationSnapshot GuiSimulationSession::snapshot() const {
    if (controller_ != nullptr) {
        return controller_->snapshot();
    }
    return SimulationSnapshot{.run_state = GuiRunState::NotConfigured,
                              .current_tick = 0,
                              .stop_tick = draft_.stop_tick(),
                              .experiment = experiment_presentation_,
                              .event_log = {},
                              .functional_model_attached =
                                  static_cast<bool>(functional_model_factory_),
                              .functional_signal_registry = functional_signal_registry_,
                              .functional_observations = {},
                              .resources = {}};
}

} // namespace cpssim
