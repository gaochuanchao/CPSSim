/***
 * File: src/cpssim/gui/selection_model.cpp
 * Purpose: Implement shared strong-identity selection and synchronization with
 *          detached simulation snapshots.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/selection_model.hpp"

#include <algorithm>

namespace cpssim {
namespace {

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
    }
    return GuiSelectionKind::None;
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
    case GuiSelectionKind::Job:
        entity_matches = event_refers_to_job(event, *selection.job());
        break;
    case GuiSelectionKind::Event:
        entity_matches = event.sequence() == *selection.event_sequence();
        break;
    case GuiSelectionKind::None:
    case GuiSelectionKind::Experiment:
    case GuiSelectionKind::Route:
        break;
    }
    return entity_matches ||
           (selection.tick_range().has_value() && selection.tick_range()->contains(event.tick()));
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
    case GuiSelectionKind::Task:
        available = find_task(snapshot.experiment, *selection.task_id()) != nullptr;
        break;
    case GuiSelectionKind::Resource:
        available = find_resource(snapshot.experiment, *selection.resource_id()) != nullptr;
        break;
    case GuiSelectionKind::Route:
        available = find_route(snapshot.experiment, *selection.route_id()) != nullptr;
        break;
    case GuiSelectionKind::Job:
        available = snapshot_contains_job(snapshot, *selection.job());
        break;
    case GuiSelectionKind::Event:
        available = std::any_of(snapshot.event_log.begin(), snapshot.event_log.end(),
                                [&selection](const Event& event) {
                                    return event.sequence() == *selection.event_sequence();
                                });
        break;
    }
    if (!available) {
        selection.clear();
    }
}

} // namespace cpssim
