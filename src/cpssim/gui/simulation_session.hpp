/***
 * File: src/cpssim/gui/simulation_session.hpp
 * Purpose: Declare GUI-neutral ownership for draft validation and atomic active
 *          controller replacement.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/draft_run_plan.hpp"
#include "cpssim/gui/simulation_controller.hpp"

#include <memory>
#include <optional>

namespace cpssim {

/*** Owns one loaded experiment, its draft plan, and optional active run. ***/
class GuiSimulationSession {
  public:
    GuiSimulationSession(ExperimentConfig config, Tick initial_stop_tick,
                         GuiFunctionalModelFactory functional_model_factory = {},
                         std::vector<GuiSignalDescriptor> functional_signal_registry = {});

    const ExperimentConfig& config() const { return config_; }
    const RunPlanDraft& draft() const { return draft_; }
    const RunPlan* active_plan() const;
    bool has_active_run() const { return controller_ != nullptr; }
    bool draft_editable() const;
    bool draft_dirty() const;

    // Mutations return false while semantic editing is disabled by Running.
    bool set_draft_stop_tick(Tick stop_tick);
    bool set_draft_policy_kind(SchedulingPolicyKind policy_kind);
    bool set_draft_assignment(TaskId task_id, std::optional<ResourceId> resource_id);

    // Atomically replaces draft fields from a plan validated for this experiment.
    bool replace_draft(const RunPlan& plan);

    // Validates without constructing runtime and retains field diagnostics.
    const RunPlanBuildResult& validate_draft();
    const std::optional<RunPlanBuildResult>& last_validation() const { return last_validation_; }

    // Replaces the controller only after validation and full construction.
    bool apply_draft();

    // Delegates active-run controls; returns false before the first Apply.
    bool enqueue(GuiCommand command);
    GuiControllerUpdateResult update(const GuiExecutionSettings& settings = {});
    SimulationSnapshot snapshot() const;
    SimulationProgress progress() const;
    RunPerformanceSummary performance_summary() const;
    GuiRunState run_state() const {
        return controller_ != nullptr ? controller_->run_state() : GuiRunState::NotConfigured;
    }
    bool has_queued_work() const noexcept {
        return controller_ != nullptr && controller_->has_queued_commands();
    }
    bool needs_update() const noexcept {
        return controller_ != nullptr &&
               (controller_->run_state() == GuiRunState::Running ||
                controller_->has_queued_commands());
    }
    std::uint64_t run_generation() const { return run_generation_; }

  private:
    void draft_changed();

    ExperimentConfig config_;
    GuiFunctionalModelFactory functional_model_factory_;
    std::vector<GuiSignalDescriptor> functional_signal_registry_;
    ExperimentPresentationSnapshot experiment_presentation_;
    RunPlanDraft draft_;
    std::optional<RunPlanBuildResult> last_validation_;
    std::unique_ptr<SimulationController> controller_;
    std::uint64_t run_generation_{0};
};

} // namespace cpssim
