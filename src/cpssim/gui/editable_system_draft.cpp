/*** Implement detached system editing and complete structured validation. ***/

#include "cpssim/gui/editable_system_draft.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cpssim {
namespace {

constexpr std::size_t system_entity_index = 0;

void add_diagnostic(SystemDraftBuildResult& result, SystemDraftDiagnosticCode code,
                    SystemDraftEntityKind kind, std::size_t index, SystemDraftField field,
                    std::string message, std::optional<TaskId> task_id = std::nullopt,
                    std::optional<ResourceId> resource_id = std::nullopt) {
    result.diagnostics.push_back({.code = code,
                                  .entity_kind = kind,
                                  .entity_index = index,
                                  .field = field,
                                  .task_id = task_id,
                                  .resource_id = resource_id,
                                  .message = std::move(message)});
}

template <typename Id, typename Rows> Id smallest_unused_id(const Rows& rows) {
    for (std::uint64_t candidate = 1;; ++candidate) {
        const auto used = std::any_of(rows.begin(), rows.end(), [candidate](const auto& row) {
            return row.id.value() == candidate;
        });
        if (!used) {
            return Id{candidate};
        }
        if (candidate == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error{"no positive system identifier remains available"};
        }
    }
}

template <typename Rows>
std::string unique_copy_name(const Rows& rows, const std::string& original) {
    const auto available = [&rows](const std::string& candidate) {
        return std::none_of(rows.begin(), rows.end(),
                            [&candidate](const auto& row) { return row.name == candidate; });
    };
    auto candidate = original + " copy";
    if (available(candidate)) {
        return candidate;
    }
    for (std::uint64_t suffix = 2;; ++suffix) {
        candidate = original + " copy " + std::to_string(suffix);
        if (available(candidate)) {
            return candidate;
        }
    }
}

template <typename Id, typename Rows> bool contains_id(const Rows& rows, Id id) {
    return std::any_of(rows.begin(), rows.end(), [id](const auto& row) { return row.id == id; });
}

} // namespace

EditableSystemDraft::EditableSystemDraft(const ExperimentConfig& config)
    : tick_period_ns_{config.tick_period().count()},
      preemption_mode_{config.scheduling().preemption_mode}, baseline_{values()} {
    resources_.reserve(config.resources().size());
    for (const auto& resource : config.resources()) {
        resources_.push_back({.id = resource.id(), .name = resource.name()});
    }
    tasks_.reserve(config.tasks().size());
    for (const auto& task : config.tasks()) {
        tasks_.push_back({.id = task.id(),
                          .name = task.name(),
                          .period = task.period(),
                          .deadline = task.deadline(),
                          .offset = task.offset(),
                          .priority = task.priority()});
    }
    profiles_.reserve(config.task_resource_profiles().size());
    for (const auto& profile : config.task_resource_profiles()) {
        profiles_.push_back({.task_id = profile.task_id,
                             .resource_id = profile.resource_id,
                             .execution_time = profile.execution_time});
    }
    routes_.reserve(config.message_routes().size());
    for (const auto& route : config.message_routes()) {
        routes_.push_back({.source_task_id = route.source_task_id,
                           .destination_task_id = route.destination_task_id,
                           .send_offset = route.send_offset,
                           .delay = route.delay});
    }
    baseline_ = values();
}

EditableSystemDraft EditableSystemDraft::minimal() {
    const ExperimentConfig config{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "resource-1"}},
        {TaskSpec{TaskId{1}, "task-1",
                  PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 0}},
        {{.task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1}},
        {}};
    return EditableSystemDraft{config};
}

EditableSystemDraft::Baseline EditableSystemDraft::values() const {
    return {.tick_period_ns = tick_period_ns_,
            .preemption_mode = preemption_mode_,
            .resources = resources_,
            .tasks = tasks_,
            .profiles = profiles_,
            .routes = routes_};
}

bool EditableSystemDraft::dirty() const {
    const auto current = values();
    return current.tick_period_ns != baseline_.tick_period_ns ||
           current.preemption_mode != baseline_.preemption_mode ||
           current.resources != baseline_.resources || current.tasks != baseline_.tasks ||
           current.profiles != baseline_.profiles || current.routes != baseline_.routes;
}

ResourceId EditableSystemDraft::allocate_resource_id() const {
    return smallest_unused_id<ResourceId>(resources_);
}

TaskId EditableSystemDraft::allocate_task_id() const { return smallest_unused_id<TaskId>(tasks_); }

ResourceId EditableSystemDraft::add_resource() {
    const auto id = allocate_resource_id();
    resources_.push_back({.id = id, .name = "resource-" + std::to_string(id.value())});
    return id;
}

ResourceId EditableSystemDraft::duplicate_resource(std::size_t index) {
    if (index >= resources_.size()) {
        throw std::out_of_range{"resource draft index is out of range"};
    }
    const auto original = resources_[index];
    const auto id = allocate_resource_id();
    resources_.push_back({.id = id, .name = unique_copy_name(resources_, original.name)});
    const auto original_profiles = profiles_;
    for (const auto& profile : original_profiles) {
        if (profile.resource_id == original.id) {
            profiles_.push_back({.task_id = profile.task_id,
                                 .resource_id = id,
                                 .execution_time = profile.execution_time});
        }
    }
    return id;
}

SystemDraftMutationResult EditableSystemDraft::remove_resource(std::size_t index) {
    if (index >= resources_.size()) {
        throw std::out_of_range{"resource draft index is out of range"};
    }
    const auto id = resources_[index].id;
    if (std::any_of(profiles_.begin(), profiles_.end(),
                    [id](const auto& profile) { return profile.resource_id == id; })) {
        return {.changed = false,
                .diagnostic = SystemDraftDiagnostic{
                    .code = SystemDraftDiagnosticCode::ReferencedEntity,
                    .entity_kind = SystemDraftEntityKind::Resource,
                    .entity_index = index,
                    .field = SystemDraftField::Collection,
                    .task_id = std::nullopt,
                    .resource_id = id,
                    .message = "remove this resource's execution profiles before deleting it"}};
    }
    resources_.erase(resources_.begin() + static_cast<std::ptrdiff_t>(index));
    return {.changed = true, .diagnostic = std::nullopt};
}

void EditableSystemDraft::set_resource_id(std::size_t index, ResourceId id) {
    if (index >= resources_.size()) {
        throw std::out_of_range{"resource draft index is out of range"};
    }
    const auto previous = resources_[index].id;
    resources_[index].id = id;
    for (auto& profile : profiles_) {
        if (profile.resource_id == previous) {
            profile.resource_id = id;
        }
    }
}

void EditableSystemDraft::set_resource_name(std::size_t index, std::string name) {
    if (index >= resources_.size()) {
        throw std::out_of_range{"resource draft index is out of range"};
    }
    resources_[index].name = std::move(name);
}

TaskId EditableSystemDraft::add_task() {
    const auto id = allocate_task_id();
    tasks_.push_back({.id = id,
                      .name = "task-" + std::to_string(id.value()),
                      .period = 10,
                      .deadline = 10,
                      .offset = 0,
                      .priority = 0});
    return id;
}

TaskId EditableSystemDraft::duplicate_task(std::size_t index) {
    if (index >= tasks_.size()) {
        throw std::out_of_range{"task draft index is out of range"};
    }
    const auto original = tasks_[index];
    const auto id = allocate_task_id();
    tasks_.push_back({.id = id,
                      .name = unique_copy_name(tasks_, original.name),
                      .period = original.period,
                      .deadline = original.deadline,
                      .offset = original.offset,
                      .priority = original.priority});
    const auto original_profiles = profiles_;
    for (const auto& profile : original_profiles) {
        if (profile.task_id == original.id) {
            profiles_.push_back({.task_id = id,
                                 .resource_id = profile.resource_id,
                                 .execution_time = profile.execution_time});
        }
    }
    return id;
}

SystemDraftMutationResult EditableSystemDraft::remove_task(std::size_t index) {
    if (index >= tasks_.size()) {
        throw std::out_of_range{"task draft index is out of range"};
    }
    const auto id = tasks_[index].id;
    const auto referenced_by_profile =
        std::any_of(profiles_.begin(), profiles_.end(),
                    [id](const auto& profile) { return profile.task_id == id; });
    const auto referenced_by_route =
        std::any_of(routes_.begin(), routes_.end(), [id](const auto& route) {
            return route.source_task_id == id || route.destination_task_id == id;
        });
    if (referenced_by_profile || referenced_by_route) {
        return {
            .changed = false,
            .diagnostic = SystemDraftDiagnostic{
                .code = SystemDraftDiagnosticCode::ReferencedEntity,
                .entity_kind = SystemDraftEntityKind::Task,
                .entity_index = index,
                .field = SystemDraftField::Collection,
                .task_id = id,
                .resource_id = std::nullopt,
                .message = "remove this task's profiles and message routes before deleting it"}};
    }
    tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(index));
    return {.changed = true, .diagnostic = std::nullopt};
}

void EditableSystemDraft::set_task_id(std::size_t index, TaskId id) {
    if (index >= tasks_.size()) {
        throw std::out_of_range{"task draft index is out of range"};
    }
    const auto previous = tasks_[index].id;
    tasks_[index].id = id;
    for (auto& profile : profiles_) {
        if (profile.task_id == previous) {
            profile.task_id = id;
        }
    }
    for (auto& route : routes_) {
        if (route.source_task_id == previous) {
            route.source_task_id = id;
        }
        if (route.destination_task_id == previous) {
            route.destination_task_id = id;
        }
    }
}

void EditableSystemDraft::set_task_name(std::size_t index, std::string name) {
    if (index >= tasks_.size()) {
        throw std::out_of_range{"task draft index is out of range"};
    }
    tasks_[index].name = std::move(name);
}

void EditableSystemDraft::set_task_timing(std::size_t index, PeriodicTimingSpec timing,
                                          Priority priority) {
    if (index >= tasks_.size()) {
        throw std::out_of_range{"task draft index is out of range"};
    }
    auto& task = tasks_[index];
    task.period = timing.period;
    task.deadline = timing.deadline;
    task.offset = timing.offset;
    task.priority = priority;
}

std::optional<Tick> EditableSystemDraft::execution_profile(TaskId task_id,
                                                           ResourceId resource_id) const {
    const auto found = std::find_if(
        profiles_.begin(), profiles_.end(), [task_id, resource_id](const auto& profile) {
            return profile.task_id == task_id && profile.resource_id == resource_id;
        });
    return found == profiles_.end() ? std::nullopt : std::optional<Tick>{found->execution_time};
}

void EditableSystemDraft::set_execution_profile(TaskId task_id, ResourceId resource_id,
                                                std::optional<Tick> execution_time) {
    const auto found = std::find_if(
        profiles_.begin(), profiles_.end(), [task_id, resource_id](const auto& profile) {
            return profile.task_id == task_id && profile.resource_id == resource_id;
        });
    if (!execution_time.has_value()) {
        if (found != profiles_.end()) {
            profiles_.erase(found);
        }
        return;
    }
    if (found == profiles_.end()) {
        profiles_.push_back(
            {.task_id = task_id, .resource_id = resource_id, .execution_time = *execution_time});
    } else {
        found->execution_time = *execution_time;
    }
}

void EditableSystemDraft::append_execution_profile(DraftExecutionProfile profile) {
    profiles_.push_back(profile);
}

std::size_t EditableSystemDraft::add_message_route(TaskId source_task_id,
                                                   TaskId destination_task_id) {
    routes_.push_back({.source_task_id = source_task_id,
                       .destination_task_id = destination_task_id,
                       .send_offset = 1,
                       .delay = 1});
    return routes_.size() - 1;
}

void EditableSystemDraft::set_message_route(std::size_t index, DraftMessageRoute route) {
    if (index >= routes_.size()) {
        throw std::out_of_range{"message-route draft index is out of range"};
    }
    routes_[index] = route;
}

void EditableSystemDraft::remove_message_route(std::size_t index) {
    if (index >= routes_.size()) {
        throw std::out_of_range{"message-route draft index is out of range"};
    }
    routes_.erase(routes_.begin() + static_cast<std::ptrdiff_t>(index));
}

SystemDraftBuildResult EditableSystemDraft::build() const {
    SystemDraftBuildResult result;
    if (tick_period_ns_ <= 0) {
        add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                       SystemDraftEntityKind::System, system_entity_index,
                       SystemDraftField::TickPeriod, "tick period must be positive");
    }
    switch (preemption_mode_) {
    case PreemptionMode::Preemptive:
    case PreemptionMode::NonPreemptive:
        break;
    default:
        add_diagnostic(result, SystemDraftDiagnosticCode::Unsupported,
                       SystemDraftEntityKind::System, system_entity_index,
                       SystemDraftField::PreemptionMode, "preemption mode is unsupported");
    }

    if (resources_.empty()) {
        add_diagnostic(result, SystemDraftDiagnosticCode::Required, SystemDraftEntityKind::System,
                       system_entity_index, SystemDraftField::Collection,
                       "at least one resource is required");
    }
    for (std::size_t index = 0; index < resources_.size(); ++index) {
        const auto& resource = resources_[index];
        if (resource.id.value() == 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::Resource, index, SystemDraftField::Id,
                           "resource ID must be positive", std::nullopt, resource.id);
        }
        if (resource.name.empty()) {
            add_diagnostic(result, SystemDraftDiagnosticCode::Required,
                           SystemDraftEntityKind::Resource, index, SystemDraftField::Name,
                           "resource name must not be empty", std::nullopt, resource.id);
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (resource.id == resources_[other].id) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::Resource, index, SystemDraftField::Id,
                               "resource ID is duplicated", std::nullopt, resource.id);
            }
            if (!resource.name.empty() && resource.name == resources_[other].name) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::Resource, index, SystemDraftField::Name,
                               "resource name is duplicated", std::nullopt, resource.id);
            }
        }
    }

    if (tasks_.empty()) {
        add_diagnostic(result, SystemDraftDiagnosticCode::Required, SystemDraftEntityKind::System,
                       system_entity_index, SystemDraftField::Collection,
                       "at least one task is required");
    }
    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        const auto& task = tasks_[index];
        if (task.id.value() == 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::Task, index, SystemDraftField::Id,
                           "task ID must be positive", task.id);
        }
        if (task.name.empty()) {
            add_diagnostic(result, SystemDraftDiagnosticCode::Required, SystemDraftEntityKind::Task,
                           index, SystemDraftField::Name, "task name must not be empty", task.id);
        }
        if (task.period <= 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::Task, index, SystemDraftField::Period,
                           "task period must be positive", task.id);
        }
        if (task.deadline <= 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::Task, index, SystemDraftField::Deadline,
                           "task deadline must be positive", task.id);
        }
        if (task.offset < 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::Negative, SystemDraftEntityKind::Task,
                           index, SystemDraftField::Offset, "task offset must not be negative",
                           task.id);
        }
        if (task.priority < 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::Negative, SystemDraftEntityKind::Task,
                           index, SystemDraftField::TaskPriority,
                           "task priority must not be negative", task.id);
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (task.id == tasks_[other].id) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::Task, index, SystemDraftField::Id,
                               "task ID is duplicated", task.id);
            }
            if (!task.name.empty() && task.name == tasks_[other].name) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::Task, index, SystemDraftField::Name,
                               "task name is duplicated", task.id);
            }
        }
    }

    for (std::size_t index = 0; index < profiles_.size(); ++index) {
        const auto& profile = profiles_[index];
        const auto task_known = contains_id(tasks_, profile.task_id);
        const auto resource_known = contains_id(resources_, profile.resource_id);
        if (!task_known) {
            add_diagnostic(result, SystemDraftDiagnosticCode::UnknownReference,
                           SystemDraftEntityKind::ExecutionProfile, index,
                           SystemDraftField::TaskReference,
                           "execution profile refers to an unknown task", profile.task_id,
                           profile.resource_id);
        }
        if (!resource_known) {
            add_diagnostic(result, SystemDraftDiagnosticCode::UnknownReference,
                           SystemDraftEntityKind::ExecutionProfile, index,
                           SystemDraftField::ResourceReference,
                           "execution profile refers to an unknown resource", profile.task_id,
                           profile.resource_id);
        }
        if (profile.execution_time <= 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::ExecutionProfile, index,
                           SystemDraftField::ExecutionTime, "execution time must be positive",
                           profile.task_id, profile.resource_id);
        }
        if (task_known) {
            const auto task =
                std::find_if(tasks_.begin(), tasks_.end(),
                             [&profile](const auto& row) { return row.id == profile.task_id; });
            if (profile.execution_time > task->deadline) {
                add_diagnostic(result, SystemDraftDiagnosticCode::ExecutionExceedsDeadline,
                               SystemDraftEntityKind::ExecutionProfile, index,
                               SystemDraftField::ExecutionTime,
                               "execution time must not exceed the task deadline", profile.task_id,
                               profile.resource_id);
            }
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (profile.task_id == profiles_[other].task_id &&
                profile.resource_id == profiles_[other].resource_id) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::ExecutionProfile, index,
                               SystemDraftField::ExecutionTime,
                               "task-resource execution profile is duplicated", profile.task_id,
                               profile.resource_id);
            }
        }
    }
    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        const auto& task = tasks_[index];
        const auto has_profile =
            std::any_of(profiles_.begin(), profiles_.end(),
                        [&task](const auto& profile) { return profile.task_id == task.id; });
        if (!has_profile) {
            add_diagnostic(result, SystemDraftDiagnosticCode::Required, SystemDraftEntityKind::Task,
                           index, SystemDraftField::ExecutionTime,
                           "task requires at least one execution profile", task.id);
        }
    }

    for (std::size_t index = 0; index < routes_.size(); ++index) {
        const auto& route = routes_[index];
        if (!contains_id(tasks_, route.source_task_id)) {
            add_diagnostic(result, SystemDraftDiagnosticCode::UnknownReference,
                           SystemDraftEntityKind::MessageRoute, index, SystemDraftField::SourceTask,
                           "message route source refers to an unknown task", route.source_task_id);
        }
        if (!contains_id(tasks_, route.destination_task_id)) {
            add_diagnostic(
                result, SystemDraftDiagnosticCode::UnknownReference,
                SystemDraftEntityKind::MessageRoute, index, SystemDraftField::DestinationTask,
                "message route destination refers to an unknown task", route.destination_task_id);
        }
        if (route.send_offset <= 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::MessageRoute, index, SystemDraftField::SendOffset,
                           "message send offset must be positive", route.source_task_id);
        }
        if (route.delay <= 0) {
            add_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::MessageRoute, index, SystemDraftField::Delay,
                           "message delay must be positive", route.source_task_id);
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (route.source_task_id == routes_[other].source_task_id &&
                route.destination_task_id == routes_[other].destination_task_id) {
                add_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                               SystemDraftEntityKind::MessageRoute, index,
                               SystemDraftField::Collection,
                               "message route endpoint pair is duplicated", route.source_task_id);
            }
        }
    }

    if (!result.diagnostics.empty()) {
        return result;
    }

    try {
        std::vector<ResourceSpec> resources;
        resources.reserve(resources_.size());
        for (const auto& resource : resources_) {
            resources.emplace_back(resource.id, resource.name);
        }
        std::vector<TaskSpec> tasks;
        tasks.reserve(tasks_.size());
        for (const auto& task : tasks_) {
            tasks.emplace_back(task.id, task.name,
                               PeriodicTimingSpec{.period = task.period,
                                                  .deadline = task.deadline,
                                                  .offset = task.offset},
                               task.priority);
        }
        std::vector<TaskResourceProfile> profiles;
        profiles.reserve(profiles_.size());
        for (const auto& profile : profiles_) {
            profiles.push_back({.task_id = profile.task_id,
                                .resource_id = profile.resource_id,
                                .execution_time = profile.execution_time});
        }
        std::vector<MessageRouteSpec> routes;
        routes.reserve(routes_.size());
        for (const auto& route : routes_) {
            routes.push_back({.source_task_id = route.source_task_id,
                              .destination_task_id = route.destination_task_id,
                              .send_offset = route.send_offset,
                              .delay = route.delay});
        }
        result.config.emplace(std::chrono::nanoseconds{tick_period_ns_},
                              SchedulingSpec{.preemption_mode = preemption_mode_},
                              std::move(resources), std::move(tasks), std::move(profiles),
                              std::move(routes));
    } catch (const std::invalid_argument& error) {
        add_diagnostic(result, SystemDraftDiagnosticCode::CanonicalValidation,
                       SystemDraftEntityKind::System, system_entity_index,
                       SystemDraftField::CanonicalConfiguration, error.what());
    }
    return result;
}

} // namespace cpssim
