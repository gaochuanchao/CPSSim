/***
 * File: src/cpssim/gui/simulation_controller.hpp
 * Purpose: Declare the GUI-neutral command queue, detached simulation
 *          snapshots, and fixed-priority presentation controller.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: This boundary depends on cpssim_core, but cpssim_core does not depend
 *        on it. No Dear ImGui, GLFW, OpenGL, or live mutable kernel container
 *        appears in this interface.
 ***/

#pragma once

#include "cpssim/gui/presentation_model.hpp"
#include "cpssim/gui/signal_series.hpp"
#include "cpssim/kernel/simulation_engine.hpp"
#include "cpssim/model/run_plan.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/policy/resource_allocator.hpp"

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

/*** Creates one fresh functional model for initial construction and Reset. ***/
using GuiFunctionalModelFactory = std::function<std::unique_ptr<FunctionalModel>()>;

/*** Enumerates the complete T17 set of user-to-simulation commands. ***/
enum class GuiCommand {
    Run,
    Pause,
    Reset,
    StepNextEvent,
};

/*** Reports whether the presentation controller is paused, running, or done. ***/
enum class GuiRunState {
    NotConfigured,
    Paused,
    Running,
    Finished,
};

/*** Stores GUI commands until the controller reaches a safe update boundary. ***/
class GuiCommandQueue {
  public:
    // Appends one user command after every command already waiting.
    void push(GuiCommand command);

    // Removes the oldest command, or returns no value when the queue is empty.
    std::optional<GuiCommand> pop();

  private:
    std::deque<GuiCommand> commands_;
};

/*** Copies the displayable state of one execution resource. ***/
struct GuiResourceSnapshot {
    ResourceId id;
    std::string name;
    std::optional<JobIdentity> running_job;
    std::vector<JobIdentity> ready_jobs;
    Tick busy_ticks;
    Tick idle_ticks;
};

/***
 * Owns a detached point-in-time copy for GUI rendering. Mutating this value
 * cannot mutate the engine; callers pass it to rendering code as const data.
 ***/
struct SimulationSnapshot {
    GuiRunState run_state;
    Tick current_tick;
    Tick stop_tick;
    ExperimentPresentationSnapshot experiment;
    std::vector<Event> event_log;
    bool functional_model_attached;
    std::vector<GuiSignalDescriptor> functional_signal_registry;
    std::vector<FunctionalObservation> functional_observations;
    std::vector<GuiResourceSnapshot> resources;
};

/***
 * Coordinates fixed-priority simulator progress for the GUI workbench. It owns
 * copied configuration and allocation results so Reset can reconstruct all
 * runtime state instead of mutating engine internals.
 ***/
class SimulationController {
  public:
    /***
     * Copies the validated experiment and accepted run plan, then constructs a
     * paused engine. The plan is defensively revalidated against this exact
     * experiment before mutable runtime state exists.
     ***/
    SimulationController(ExperimentConfig config, RunPlan run_plan,
                         GuiFunctionalModelFactory functional_model_factory = {},
                         std::vector<GuiSignalDescriptor> functional_signal_registry = {});

    // Queues one control action without directly changing simulation state.
    void enqueue(GuiCommand command);

    /***
     * Applies all waiting commands in FIFO order. If still Running afterward,
     * processes one complete logical event tick, independent of render timing.
     ***/
    void update();

    // Returns detached experiment, runtime, and trace presentation values.
    SimulationSnapshot snapshot() const;

    // Returns the immutable input used by Reset and active-plan presentation.
    const RunPlan& run_plan() const { return run_plan_; }

    // Returns controller state without copying the detached trace snapshot.
    GuiRunState run_state() const { return run_state_; }

  private:
    // Reconstructs policy and engine from stored immutable experiment input.
    void reset();

    // Processes one event tick and updates Finished state when appropriate.
    void step_once();

    ExperimentConfig config_;
    RunPlan run_plan_;
    ConfiguredResourceAllocator allocator_;
    GuiFunctionalModelFactory functional_model_factory_;
    std::vector<GuiSignalDescriptor> functional_signal_registry_;
    ExperimentPresentationSnapshot experiment_presentation_;
    GuiCommandQueue commands_;
    GuiRunState run_state_{GuiRunState::Paused};
    std::unique_ptr<FixedPriorityPolicy> policy_;
    std::unique_ptr<FunctionalModel> functional_model_;
    std::unique_ptr<SimulationEngine> engine_;
};

} // namespace cpssim
