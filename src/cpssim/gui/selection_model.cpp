/***
 * File: src/cpssim/gui/selection_model.cpp
 * Purpose: Implement shared strong-identity selection and synchronization with
 *          detached simulation snapshots.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/selection_model.hpp"

#include <algorithm>
#include <utility>

namespace cpssim {
namespace {

template <typename Rows, typename Predicate> bool contains_row(const Rows& rows, Predicate match) {
    return std::any_of(rows.begin(), rows.end(), std::move(match));
}

/*** Reports whether an event refers to one complete task-local job identity. ***/
bool event_refers_to_job(const Event& event, JobIdentity job) {
    return event.entities().task_id == job.task_id() && event.entities().job_id == job.job_id();
}

/*** Reports whether a job remains present in runtime state or canonical trace. ***/
bool snapshot_contains_job(const SimulationSnapshot& snapshot, JobIdentity job) {
    for (const auto& resource : snapshot.resources) {
        if (resource.running_job == job ||
            std::find(resource.ready_jobs.begin(), resource.ready_jobs.end(), job) !=
                resource.ready_jobs.end()) {
            return true;
        }
    }
    return std::any_of(snapshot.event_log.begin(), snapshot.event_log.end(),
                       [job](const Event& event) { return event_refers_to_job(event, job); });
}

} // namespace

StructuralSelectionKind StructuralSelection::kind() const {
    switch (value_.index()) {
    case 0:
        return StructuralSelectionKind::System;
    case 1:
        return StructuralSelectionKind::Section;
    case 2:
        return StructuralSelectionKind::Resource;
    case 3:
        return StructuralSelectionKind::Task;
    case 4:
        return StructuralSelectionKind::ExecutionProfile;
    case 5:
        return StructuralSelectionKind::MessageRoute;
    case 6:
        return StructuralSelectionKind::Connection;
    default:
        return StructuralSelectionKind::System;
    }
}

void StructuralSelection::select_system() { value_ = StructuralSystemSelection{}; }
void StructuralSelection::select_section(StructuralSection section) { value_ = section; }
void StructuralSelection::select_resource(ResourceId resource_id) { value_ = resource_id; }
void StructuralSelection::select_task(TaskId task_id) { value_ = task_id; }
void StructuralSelection::select_execution_profile(DraftExecutionProfileKey profile) {
    value_ = profile;
}
void StructuralSelection::select_message_route(DraftMessageRouteKey route) { value_ = route; }
void StructuralSelection::select_connection(GuiConnectionId connection) { value_ = connection; }

std::optional<StructuralSection> StructuralSelection::section() const {
    if (const auto* value = std::get_if<StructuralSection>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<ResourceId> StructuralSelection::resource_id() const {
    if (const auto* value = std::get_if<ResourceId>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<TaskId> StructuralSelection::task_id() const {
    if (const auto* value = std::get_if<TaskId>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<DraftExecutionProfileKey> StructuralSelection::execution_profile() const {
    if (const auto* value = std::get_if<DraftExecutionProfileKey>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<DraftMessageRouteKey> StructuralSelection::message_route() const {
    if (const auto* value = std::get_if<DraftMessageRouteKey>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<GuiConnectionId> StructuralSelection::connection() const {
    if (const auto* value = std::get_if<GuiConnectionId>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

void synchronize_structural_selection(StructuralSelection& selection,
                                      const EditableSystemDraft& draft) {
    switch (selection.kind()) {
    case StructuralSelectionKind::System:
    case StructuralSelectionKind::Section:
        return;
    case StructuralSelectionKind::Resource: {
        const auto resource_id = selection.resource_id();
        if (!resource_id.has_value() ||
            !contains_row(draft.resources(),
                          [resource_id](const auto& row) { return row.id == *resource_id; })) {
            selection.select_section(StructuralSection::Resources);
        }
        return;
    }
    case StructuralSelectionKind::Task: {
        const auto task_id = selection.task_id();
        if (!task_id.has_value() || !contains_row(draft.tasks(), [task_id](const auto& row) {
                return row.id == *task_id;
            })) {
            selection.select_section(StructuralSection::Tasks);
        }
        return;
    }
    case StructuralSelectionKind::ExecutionProfile: {
        const auto key = selection.execution_profile();
        if (!key.has_value() ||
            !draft.execution_profile(key->task_id, key->resource_id).has_value()) {
            selection.select_section(StructuralSection::ExecutionProfiles);
        }
        return;
    }
    case StructuralSelectionKind::MessageRoute: {
        const auto key = selection.message_route();
        if (!key.has_value() || !contains_row(draft.routes(), [&key](const auto& row) {
                return row.source_task_id == key->source_task_id &&
                       row.destination_task_id == key->destination_task_id;
            })) {
            selection.select_section(StructuralSection::MessageRoutes);
        }
        return;
    }
    case StructuralSelectionKind::Connection: {
        const auto connection = selection.connection();
        if (!connection.has_value()) {
            selection.select_system();
            return;
        }
        const auto task_exists = [&draft](TaskId id) {
            return contains_row(draft.tasks(), [id](const auto& row) { return row.id == id; });
        };
        if (!task_exists(connection->source_task_id) ||
            !task_exists(connection->destination_task_id)) {
            selection.select_system();
            return;
        }
        if (connection->kind == GuiConnectionKind::Communication &&
            !contains_row(draft.routes(), [&connection](const auto& row) {
                return row.source_task_id == connection->source_task_id &&
                       row.destination_task_id == connection->destination_task_id;
            })) {
            selection.select_section(StructuralSection::MessageRoutes);
        }
        return;
    }
    }
}

/*** Returns the active alternative's stable entity domain. ***/
GuiSelectionKind GuiSelection::kind() const {
    switch (value_.index()) {
    case 0:
        return GuiSelectionKind::None;
    case 1:
        return GuiSelectionKind::Experiment;
    case 2:
        return GuiSelectionKind::Task;
    case 3:
        return GuiSelectionKind::Resource;
    case 4:
        return GuiSelectionKind::Route;
    case 5:
        return GuiSelectionKind::Job;
    case 6:
        return GuiSelectionKind::Event;
    default:
        return GuiSelectionKind::None;
    }
}

void GuiSelection::clear() {
    value_ = std::monostate{};
    tick_range_.reset();
}
void GuiSelection::select_experiment() { value_ = GuiExperimentSelection{}; }
void GuiSelection::select_task(TaskId task_id) { value_ = task_id; }
void GuiSelection::select_resource(ResourceId resource_id) { value_ = resource_id; }
void GuiSelection::select_route(GuiRouteIdentity route_id) { value_ = route_id; }
void GuiSelection::select_job(JobIdentity job) { value_ = job; }
void GuiSelection::select_event(EventSequence event_sequence) { value_ = event_sequence; }
void GuiSelection::select_tick(Tick tick) { tick_range_ = GuiTickRange{tick, tick}; }
void GuiSelection::select_tick_range(GuiTickRange range) {
    if (range.end_tick < range.begin_tick) {
        std::swap(range.begin_tick, range.end_tick);
    }
    tick_range_ = range;
}
void GuiSelection::clear_tick_range() { tick_range_.reset(); }

std::optional<TaskId> GuiSelection::task_id() const {
    if (const auto* value = std::get_if<TaskId>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<ResourceId> GuiSelection::resource_id() const {
    if (const auto* value = std::get_if<ResourceId>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<GuiRouteIdentity> GuiSelection::route_id() const {
    if (const auto* value = std::get_if<GuiRouteIdentity>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<JobIdentity> GuiSelection::job() const {
    if (const auto* value = std::get_if<JobIdentity>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<EventSequence> GuiSelection::event_sequence() const {
    if (const auto* value = std::get_if<EventSequence>(&value_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<GuiTickRange> GuiSelection::tick_range() const { return tick_range_; }

/*** Matches selection against the event's stable identity references. ***/
bool event_matches_selection(const Event& event, const GuiSelection& selection) {
    auto entity_matches = false;
    switch (selection.kind()) {
    case GuiSelectionKind::Task:
        entity_matches = event.entities().task_id == selection.task_id();
        break;
    case GuiSelectionKind::Resource:
        entity_matches = event.entities().resource_id == selection.resource_id();
        break;
    case GuiSelectionKind::Job: {
        const auto job = selection.job();
        entity_matches = job.has_value() && event_refers_to_job(event, job.value());
        break;
    }
    case GuiSelectionKind::Event:
        entity_matches = event.sequence() == selection.event_sequence();
        break;
    case GuiSelectionKind::None:
    case GuiSelectionKind::Experiment:
    case GuiSelectionKind::Route:
        break;
    }
    const auto tick_range = selection.tick_range();
    return entity_matches || (tick_range.has_value() && tick_range.value().contains(event.tick()));
}

/*** Clears only identities no longer present after reset/config replacement. ***/
void synchronize_selection(GuiSelection& selection, const SimulationSnapshot& snapshot) {
    if (const auto range = selection.tick_range();
        range.has_value() && (range->begin_tick < 0 || range->end_tick < range->begin_tick ||
                              range->end_tick > snapshot.current_tick)) {
        selection.clear_tick_range();
    }

    bool available = true;
    switch (selection.kind()) {
    case GuiSelectionKind::None:
    case GuiSelectionKind::Experiment:
        return;
    case GuiSelectionKind::Task: {
        const auto task_id = selection.task_id();
        available =
            task_id.has_value() && find_task(snapshot.experiment, task_id.value()) != nullptr;
        break;
    }
    case GuiSelectionKind::Resource: {
        const auto resource_id = selection.resource_id();
        available = resource_id.has_value() &&
                    find_resource(snapshot.experiment, resource_id.value()) != nullptr;
        break;
    }
    case GuiSelectionKind::Route: {
        const auto route_id = selection.route_id();
        available =
            route_id.has_value() && find_route(snapshot.experiment, route_id.value()) != nullptr;
        break;
    }
    case GuiSelectionKind::Job: {
        const auto job = selection.job();
        available = job.has_value() && snapshot_contains_job(snapshot, job.value());
        break;
    }
    case GuiSelectionKind::Event:
        available = std::any_of(snapshot.event_log.begin(), snapshot.event_log.end(),
                                [&selection](const Event& event) {
                                    return event.sequence() == selection.event_sequence();
                                });
        break;
    }
    if (!available) {
        selection.clear();
    }
}

} // namespace cpssim
