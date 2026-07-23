/*** Implement deterministic Explorer create/duplicate/confirmed-delete behavior. ***/

#include "cpssim/gui/system_builder_interaction.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace cpssim {
namespace {

SystemExplorerActionResult select_created(StructuralSection section, StructuralSelection selection,
                                          SystemBuilderFocusTarget focus) {
    return {.changed = true,
            .expand_section = section,
            .scroll_to = selection,
            .focus = focus,
            .diagnostic = {}};
}

SystemExplorerActionResult failed_action(std::string diagnostic) {
    return {.changed = false,
            .expand_section = std::nullopt,
            .scroll_to = std::nullopt,
            .focus = SystemBuilderFocusTarget::None,
            .diagnostic = std::move(diagnostic)};
}

std::size_t assignment_count(const std::vector<DraftTaskAssignment>& assignments,
                             const StructuralSelection& target) {
    if (const auto task_id = target.task_id(); task_id.has_value()) {
        return static_cast<std::size_t>(std::count_if(
            assignments.begin(), assignments.end(),
            [task_id](const auto& assignment) { return assignment.task_id == *task_id; }));
    }
    if (const auto resource_id = target.resource_id(); resource_id.has_value()) {
        return static_cast<std::size_t>(std::count_if(
            assignments.begin(), assignments.end(), [resource_id](const auto& assignment) {
                return assignment.resource_id == *resource_id;
            }));
    }
    return 0;
}

template <typename Rows, typename Id, typename Select>
void select_neighbor(const Rows& rows, Id removed_id, StructuralSelection& selection,
                     StructuralSection parent, Select select) {
    const auto removed = std::find_if(
        rows.begin(), rows.end(), [removed_id](const auto& row) { return row.id == removed_id; });
    if (rows.size() <= 1 || removed == rows.end()) {
        selection.select_section(parent);
        return;
    }
    const auto index = static_cast<std::size_t>(std::distance(rows.begin(), removed));
    const auto neighbor = index + 1 < rows.size() ? index + 1 : index - 1;
    select(selection, rows[neighbor].id);
}

void erase_task_assignment(std::vector<DraftTaskAssignment>& assignments, TaskId task_id) {
    std::erase_if(assignments,
                  [task_id](const auto& assignment) { return assignment.task_id == task_id; });
}

void clear_resource_assignments(std::vector<DraftTaskAssignment>& assignments,
                                ResourceId resource_id) {
    for (auto& assignment : assignments) {
        if (assignment.resource_id == resource_id) {
            assignment.resource_id.reset();
        }
    }
}

} // namespace

SystemBuilderInteractionResult create_message_route(TaskId source, TaskId destination,
                                                    EditableSystemDraft& draft,
                                                    StructuralSelection& selection) {
    // Validate source exists
    const auto source_found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                           [source](const auto& task) { return task.id == source; });
    if (source_found == draft.tasks().end()) {
        return {.changed = false,
                .diagnostic = "Source task does not exist in the system draft."};
    }

    // Validate destination exists
    const auto dest_found =
        std::find_if(draft.tasks().begin(), draft.tasks().end(),
                     [destination](const auto& task) { return task.id == destination; });
    if (dest_found == draft.tasks().end()) {
        return {.changed = false,
                .diagnostic = "Destination task does not exist in the system draft."};
    }

    // Reject duplicate ordered endpoint pair
    const auto duplicate = std::any_of(draft.routes().begin(), draft.routes().end(),
                                       [source, destination](const auto& route) {
                                           return route.source_task_id == source &&
                                                  route.destination_task_id == destination;
                                       });
    if (duplicate) {
        return {.changed = false,
                .diagnostic = "A message route between these tasks already exists."};
    }

    // Self-loops are currently valid per domain policy — no explicit rejection.
    // If policy changes, add the check here.

    // Create the route using existing domain defaults (send_offset=1, delay=1)
    static_cast<void>(draft.add_message_route(source, destination));

    // Select the canonical Communication GuiConnectionId.
    // This ensures the route is immediately editable and deletable through the
    // Architecture view without requiring a second click.
    selection.select_connection(
        GuiConnectionId{GuiConnectionKind::Communication, source, destination});

    return {.changed = true, .diagnostic = {}};
}

SystemEntityCreateAvailability
SystemExplorerInteraction::create_availability(StructuralSection section,
                                               const EditableSystemDraft& draft) const {
    switch (section) {
    case StructuralSection::Resources:
    case StructuralSection::Tasks:
        return {.available = true, .explanation = {}};
    case StructuralSection::ExecutionProfiles:
        return draft.next_execution_profile().has_value()
                   ? SystemEntityCreateAvailability{.available = true, .explanation = {}}
                   : SystemEntityCreateAvailability{
                         .available = false,
                         .explanation = "Add a task/resource or remove an existing profile first."};
    case StructuralSection::MessageRoutes:
        if (draft.tasks().empty()) {
            return {.available = false,
                    .explanation = "Add at least one task before creating a route."};
        }
        return draft.next_message_route().has_value()
                   ? SystemEntityCreateAvailability{.available = true, .explanation = {}}
                   : SystemEntityCreateAvailability{
                         .available = false,
                         .explanation = "Every task endpoint pair already has a route."};
    }
    return {.available = false, .explanation = "Unsupported structural section."};
}

SystemExplorerActionResult SystemExplorerInteraction::create(StructuralSection section,
                                                             EditableSystemDraft& draft,
                                                             StructuralSelection& selection) {
    const auto available = create_availability(section, draft);
    if (!available.available) {
        return failed_action(available.explanation);
    }
    switch (section) {
    case StructuralSection::Resources: {
        selection.select_resource(draft.add_resource());
        return select_created(section, selection, SystemBuilderFocusTarget::ResourceName);
    }
    case StructuralSection::Tasks: {
        selection.select_task(draft.add_task());
        return select_created(section, selection, SystemBuilderFocusTarget::TaskName);
    }
    case StructuralSection::ExecutionProfiles: {
        const auto profile = draft.add_execution_profile();
        if (!profile.has_value()) {
            return failed_action("No unused task-resource profile combination is available.");
        }
        selection.select_execution_profile(profile.value());
        return select_created(section, selection, SystemBuilderFocusTarget::ProfileExecutionTime);
    }
    case StructuralSection::MessageRoutes: {
        const auto route = draft.add_message_route();
        if (!route.has_value()) {
            return failed_action("No unused message-route endpoint pair is available.");
        }
        selection.select_connection(
            GuiConnectionId{GuiConnectionKind::Communication, route->source_task_id,
                           route->destination_task_id});
        return select_created(section, selection, SystemBuilderFocusTarget::RouteSource);
    }
    }
    return failed_action("Unsupported structural section.");
}

SystemExplorerActionResult SystemExplorerInteraction::duplicate(const StructuralSelection& target,
                                                                EditableSystemDraft& draft,
                                                                StructuralSelection& selection) {
    switch (target.kind()) {
    case StructuralSelectionKind::Resource: {
        const auto id = target.resource_id();
        const auto found = std::find_if(draft.resources().begin(), draft.resources().end(),
                                        [id](const auto& row) { return id == row.id; });
        if (found == draft.resources().end()) {
            break;
        }
        selection.select_resource(draft.duplicate_resource(
            static_cast<std::size_t>(std::distance(draft.resources().begin(), found))));
        return select_created(StructuralSection::Resources, selection,
                              SystemBuilderFocusTarget::ResourceName);
    }
    case StructuralSelectionKind::Task: {
        const auto id = target.task_id();
        const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                        [id](const auto& row) { return id == row.id; });
        if (found == draft.tasks().end()) {
            break;
        }
        selection.select_task(draft.duplicate_task(
            static_cast<std::size_t>(std::distance(draft.tasks().begin(), found))));
        return select_created(StructuralSection::Tasks, selection,
                              SystemBuilderFocusTarget::TaskName);
    }
    case StructuralSelectionKind::ExecutionProfile: {
        const auto profile = target.execution_profile();
        if (!profile.has_value()) {
            break;
        }
        const auto duplicated = draft.duplicate_execution_profile(profile.value());
        if (!duplicated.has_value()) {
            return failed_action(
                create_availability(StructuralSection::ExecutionProfiles, draft).explanation);
        }
        selection.select_execution_profile(*duplicated);
        return select_created(StructuralSection::ExecutionProfiles, selection,
                              SystemBuilderFocusTarget::ProfileExecutionTime);
    }
    case StructuralSelectionKind::MessageRoute: {
        const auto route = target.message_route();
        if (!route.has_value()) {
            break;
        }
        const auto duplicated = draft.duplicate_message_route(route.value());
        if (!duplicated.has_value()) {
            return failed_action(
                create_availability(StructuralSection::MessageRoutes, draft).explanation);
        }
        selection.select_connection(
            GuiConnectionId{GuiConnectionKind::Communication, duplicated->source_task_id,
                           duplicated->destination_task_id});
        return select_created(StructuralSection::MessageRoutes, selection,
                              SystemBuilderFocusTarget::RouteSource);
    }
    case StructuralSelectionKind::System:
    case StructuralSelectionKind::Section:
    case StructuralSelectionKind::Connection:
        break;
    }
    return failed_action("The selected structural entity is unavailable.");
}

bool SystemExplorerInteraction::request_delete(
    const StructuralSelection& target, const EditableSystemDraft& draft,
    const std::vector<DraftTaskAssignment>& assignments) {
    SystemDeletionImpact impact{.target = target,
                                .structural = {},
                                .run_plan_assignments = assignment_count(assignments, target)};
    switch (target.kind()) {
    case StructuralSelectionKind::Resource: {
        const auto resource_id = target.resource_id();
        if (!resource_id.has_value()) {
            return false;
        }
        impact.structural = draft.resource_cascade_impact(resource_id.value());
        break;
    }
    case StructuralSelectionKind::Task: {
        const auto task_id = target.task_id();
        if (!task_id.has_value()) {
            return false;
        }
        impact.structural = draft.task_cascade_impact(task_id.value());
        break;
    }
    case StructuralSelectionKind::ExecutionProfile:
    case StructuralSelectionKind::MessageRoute:
        break;
    case StructuralSelectionKind::System:
    case StructuralSelectionKind::Section:
    case StructuralSelectionKind::Connection:
        return false;
    }
    pending_delete_ = impact;
    return true;
}

SystemExplorerActionResult
SystemExplorerInteraction::confirm_delete(EditableSystemDraft& draft,
                                          std::vector<DraftTaskAssignment>& assignments,
                                          StructuralSelection& selection) {
    if (!pending_delete_.has_value()) {
        return failed_action("No structural deletion is pending.");
    }
    const auto target = pending_delete_->target;
    pending_delete_.reset();
    auto changed = false;
    switch (target.kind()) {
    case StructuralSelectionKind::Resource: {
        const auto resource_id = target.resource_id();
        if (!resource_id.has_value()) {
            break;
        }
        const auto id = resource_id.value();
        select_neighbor(draft.resources(), id, selection, StructuralSection::Resources,
                        [](auto& selected, auto neighbor) { selected.select_resource(neighbor); });
        changed = draft.cascade_remove_resource(id);
        clear_resource_assignments(assignments, id);
        break;
    }
    case StructuralSelectionKind::Task: {
        const auto task_id = target.task_id();
        if (!task_id.has_value()) {
            break;
        }
        const auto id = task_id.value();
        select_neighbor(draft.tasks(), id, selection, StructuralSection::Tasks,
                        [](auto& selected, auto neighbor) { selected.select_task(neighbor); });
        changed = draft.cascade_remove_task(id);
        erase_task_assignment(assignments, id);
        break;
    }
    case StructuralSelectionKind::ExecutionProfile: {
        const auto profile = target.execution_profile();
        changed = profile.has_value() && draft.remove_execution_profile(profile.value());
        selection.select_section(StructuralSection::ExecutionProfiles);
        break;
    }
    case StructuralSelectionKind::MessageRoute: {
        const auto route = target.message_route();
        changed = route.has_value() && draft.remove_message_route(route.value());
        selection.select_section(StructuralSection::MessageRoutes);
        break;
    }
    case StructuralSelectionKind::System:
    case StructuralSelectionKind::Section:
    case StructuralSelectionKind::Connection:
        break;
    }
    synchronize_structural_selection(selection, draft);
    return {.changed = changed,
            .expand_section = std::nullopt,
            .scroll_to = std::nullopt,
            .focus = SystemBuilderFocusTarget::None,
            .diagnostic = changed ? std::string{} : "The selected entity is unavailable."};
}

} // namespace cpssim
