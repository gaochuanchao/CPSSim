/***
 * File: src/cpssim/gui/editable_system_draft.hpp
 * Purpose: Declare a GUI-independent editable system configuration draft.
 * Creator: OpenAI Codex
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include "cpssim/model/experiment_config.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

enum class SystemDraftEntityKind {
    System,
    Resource,
    Task,
    ExecutionProfile,
    MessageRoute,
};

enum class SystemDraftField {
    Collection,
    TickPeriod,
    PreemptionMode,
    Id,
    Name,
    Period,
    Deadline,
    Offset,
    TaskPriority,
    TaskReference,
    ResourceReference,
    ExecutionTime,
    SourceTask,
    DestinationTask,
    SendOffset,
    Delay,
    CanonicalConfiguration,
};

enum class SystemDraftDiagnosticCode {
    Required,
    Unsupported,
    NonPositive,
    Negative,
    Duplicate,
    UnknownReference,
    ExecutionExceedsDeadline,
    ReferencedEntity,
    CanonicalValidation,
};

/*** Addresses one system-draft issue to an entity row and field. ***/
struct SystemDraftDiagnostic {
    SystemDraftDiagnosticCode code;
    SystemDraftEntityKind entity_kind;
    std::size_t entity_index;
    SystemDraftField field;
    std::optional<TaskId> task_id;
    std::optional<ResourceId> resource_id;
    std::string message;

    bool operator==(const SystemDraftDiagnostic&) const = default;
};

struct DraftResource {
    ResourceId id;
    std::string name;

    bool operator==(const DraftResource&) const = default;
};

struct DraftTask {
    TaskId id;
    std::string name;
    Tick period;
    Tick deadline;
    Tick offset;
    Priority priority;

    bool operator==(const DraftTask&) const = default;
};

struct DraftExecutionProfile {
    TaskId task_id;
    ResourceId resource_id;
    Tick execution_time;

    bool operator==(const DraftExecutionProfile&) const = default;
};

struct DraftMessageRoute {
    TaskId source_task_id;
    TaskId destination_task_id;
    Tick send_offset;
    Tick delay;

    bool operator==(const DraftMessageRoute&) const = default;
};

struct SystemDraftBuildResult {
    std::optional<ExperimentConfig> config;
    std::vector<SystemDraftDiagnostic> diagnostics;

    bool valid() const { return config.has_value(); }
};

struct SystemDraftMutationResult {
    bool changed{false};
    std::optional<SystemDraftDiagnostic> diagnostic;
};

/*** Owns detached editable fields and a typed baseline for derived dirty state. ***/
class EditableSystemDraft {
  public:
    explicit EditableSystemDraft(const ExperimentConfig& config);

    // Creates the smallest valid system supported by the existing model.
    static EditableSystemDraft minimal();

    std::int64_t tick_period_ns() const { return tick_period_ns_; }
    PreemptionMode preemption_mode() const { return preemption_mode_; }
    const std::vector<DraftResource>& resources() const { return resources_; }
    const std::vector<DraftTask>& tasks() const { return tasks_; }
    const std::vector<DraftExecutionProfile>& profiles() const { return profiles_; }
    const std::vector<DraftMessageRoute>& routes() const { return routes_; }

    bool dirty() const;
    SystemDraftBuildResult build() const;

    void set_tick_period_ns(std::int64_t value) { tick_period_ns_ = value; }
    void set_preemption_mode(PreemptionMode value) { preemption_mode_ = value; }

    ResourceId add_resource();
    ResourceId duplicate_resource(std::size_t index);
    SystemDraftMutationResult remove_resource(std::size_t index);
    void set_resource_id(std::size_t index, ResourceId id);
    void set_resource_name(std::size_t index, std::string name);

    TaskId add_task();
    TaskId duplicate_task(std::size_t index);
    SystemDraftMutationResult remove_task(std::size_t index);
    void set_task_id(std::size_t index, TaskId id);
    void set_task_name(std::size_t index, std::string name);
    void set_task_timing(std::size_t index, PeriodicTimingSpec timing, Priority priority);

    std::optional<Tick> execution_profile(TaskId task_id, ResourceId resource_id) const;
    void set_execution_profile(TaskId task_id, ResourceId resource_id,
                               std::optional<Tick> execution_time);
    void append_execution_profile(DraftExecutionProfile profile);

    std::size_t add_message_route(TaskId source_task_id, TaskId destination_task_id);
    void set_message_route(std::size_t index, DraftMessageRoute route);
    void remove_message_route(std::size_t index);

  private:
    struct Baseline {
        std::int64_t tick_period_ns;
        PreemptionMode preemption_mode;
        std::vector<DraftResource> resources;
        std::vector<DraftTask> tasks;
        std::vector<DraftExecutionProfile> profiles;
        std::vector<DraftMessageRoute> routes;
    };

    Baseline values() const;
    ResourceId allocate_resource_id() const;
    TaskId allocate_task_id() const;

    std::int64_t tick_period_ns_;
    PreemptionMode preemption_mode_;
    std::vector<DraftResource> resources_;
    std::vector<DraftTask> tasks_;
    std::vector<DraftExecutionProfile> profiles_;
    std::vector<DraftMessageRoute> routes_;
    Baseline baseline_;
};

} // namespace cpssim
