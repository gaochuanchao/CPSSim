/***
 * File: src/cpssim/gui/selection_model.hpp
 * Purpose: Declare the shared strong-identity GUI selection value and its
 *          snapshot synchronization helpers.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Selection is presentation state. Changing it never enqueues a
 *        simulator command or mutates runtime state.
 ***/

#pragma once

#include "cpssim/gui/editable_system_draft.hpp"
#include "cpssim/gui/simulation_controller.hpp"

#include <optional>
#include <variant>

namespace cpssim {

enum class StructuralSection {
    Resources,
    Tasks,
    ExecutionProfiles,
    MessageRoutes,
};

enum class StructuralSelectionKind {
    System,
    Section,
    Resource,
    Task,
    ExecutionProfile,
    MessageRoute,
};

struct StructuralSystemSelection {
    bool operator==(const StructuralSystemSelection&) const = default;
};

/*** Owns the Explorer selection independently of runtime observation selection. ***/
class StructuralSelection {
  public:
    StructuralSelectionKind kind() const;

    void select_system();
    void select_section(StructuralSection section);
    void select_resource(ResourceId resource_id);
    void select_task(TaskId task_id);
    void select_execution_profile(DraftExecutionProfileKey profile);
    void select_message_route(DraftMessageRouteKey route);

    std::optional<StructuralSection> section() const;
    std::optional<ResourceId> resource_id() const;
    std::optional<TaskId> task_id() const;
    std::optional<DraftExecutionProfileKey> execution_profile() const;
    std::optional<DraftMessageRouteKey> message_route() const;

    bool operator==(const StructuralSelection&) const = default;

  private:
    using Value = std::variant<StructuralSystemSelection, StructuralSection, ResourceId, TaskId,
                               DraftExecutionProfileKey, DraftMessageRouteKey>;
    Value value_{StructuralSystemSelection{}};
};

// Repairs only structural identity against the detached draft.
void synchronize_structural_selection(StructuralSelection& selection,
                                      const EditableSystemDraft& draft);

/*** Distinguishes the stable entity domain currently selected by the GUI. ***/
enum class GuiSelectionKind {
    None,
    Experiment,
    Task,
    Resource,
    Route,
    Job,
    Event,
};

/*** Marker value for selecting the experiment root. ***/
struct GuiExperimentSelection {
    bool operator==(const GuiExperimentSelection&) const = default;
};

/*** Inclusive logical tick range selected independently of an entity. ***/
struct GuiTickRange {
    Tick begin_tick;
    Tick end_tick;

    bool contains(Tick tick) const { return tick >= begin_tick && tick <= end_tick; }
    bool operator==(const GuiTickRange&) const = default;
};

/*** Owns one selected entity and an optional independent logical tick range. ***/
class GuiSelection {
  public:
    GuiSelectionKind kind() const;

    void clear();
    void select_experiment();
    void select_task(TaskId task_id);
    void select_resource(ResourceId resource_id);
    void select_route(GuiRouteIdentity route_id);
    void select_job(JobIdentity job);
    void select_event(EventSequence event_sequence);
    void select_tick(Tick tick);
    void select_tick_range(GuiTickRange range);
    void clear_tick_range();

    std::optional<TaskId> task_id() const;
    std::optional<ResourceId> resource_id() const;
    std::optional<GuiRouteIdentity> route_id() const;
    std::optional<JobIdentity> job() const;
    std::optional<EventSequence> event_sequence() const;
    std::optional<GuiTickRange> tick_range() const;

  private:
    using Value = std::variant<std::monostate, GuiExperimentSelection, TaskId, ResourceId,
                               GuiRouteIdentity, JobIdentity, EventSequence>;
    Value value_;
    std::optional<GuiTickRange> tick_range_;
};

// Reports whether one event is the selected event or refers to the selection.
bool event_matches_selection(const Event& event, const GuiSelection& selection);

// Clears a selection only when its stable identity is absent from the snapshot.
void synchronize_selection(GuiSelection& selection, const SimulationSnapshot& snapshot);

} // namespace cpssim
