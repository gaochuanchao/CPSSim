/*** Implement the shared structural edit controller. ***/
#include "apps/qt_gui/structural_edit_controller.hpp"

#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/project/system_edit_policy.hpp"
#include "cpssim/application/workbench_application.hpp"
#include "cpssim/gui/draft_run_plan.hpp"
#include "cpssim/gui/editable_system_draft.hpp"
#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/system_builder_interaction.hpp"

#include <QUndoCommand>

#include <stdexcept>
#include <utility>

namespace cpssim::qt {
namespace {

struct DraftState {
    EditableSystemDraft draft;
    std::vector<DraftTaskAssignment> assignments;
    StructuralSelection selection;
};

class RestoreDraftCommand final : public QUndoCommand {
  public:
    RestoreDraftCommand(QtWorkbenchBridge& bridge, DraftState before, DraftState after,
                        QString text)
        : QUndoCommand(std::move(text)), bridge_{bridge}, before_{std::move(before)},
          after_{std::move(after)} {}

    void undo() override { restore(before_); }
    void redo() override { restore(after_); }

  private:
    void restore(const DraftState& state) {
        bridge_.restore_draft(state.draft, state.assignments, state.selection);
    }

    QtWorkbenchBridge& bridge_;
    DraftState before_;
    DraftState after_;
};

} // namespace

QtStructuralEditController::QtStructuralEditController(QtWorkbenchBridge& bridge,
                                                       QObject* parent)
    : QObject(parent), bridge_{bridge}, undo_stack_{this} {}

QUndoStack& QtStructuralEditController::undo_stack() noexcept { return undo_stack_; }

bool QtStructuralEditController::editing_enabled() const {
    return bridge_.application().editable_system().has_value() &&
           bridge_.application().run_state() != GuiRunState::Running;
}

ProjectSystemEditPolicy QtStructuralEditController::edit_policy() const {
    const auto& application = bridge_.application();
    return application.has_active_project()
               ? project_system_edit_policy(application.active_project().metadata())
               : ProjectSystemEditPolicy::ReadOnlyAdapter;
}

void QtStructuralEditController::synchronize_active_project() {
    const auto& application = bridge_.application();
    const auto active_root = application.has_active_project()
                                 ? std::optional{application.active_project().root()}
                                 : std::nullopt;
    if (active_root != undo_project_root_) {
        undo_stack_.clear();
        undo_project_root_ = active_root;
    }
}

bool QtStructuralEditController::apply(const QString& command_text, DraftMutator mutator) {
    auto& application = bridge_.application();
    if (!application.editable_system().has_value())
        return false;
    if (!editing_enabled())
        return false;
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;
    try {
        mutator(after.draft, after.assignments, after.selection);
        undo_stack_.push(
            new RestoreDraftCommand{bridge_, std::move(before), std::move(after), command_text});
        return true;
    } catch (const std::exception& error) {
        application.set_status(error.what(), true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
}

bool QtStructuralEditController::create_component(StructuralSection section) {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value())
        return false;
    if ((section == StructuralSection::Tasks || section == StructuralSection::MessageRoutes) &&
        edit_policy() != ProjectSystemEditPolicy::Generic) {
        application.set_status("Adapter-owned tasks and route structure are protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;

    // Record the previous last resource before creating the new one
    std::optional<ResourceId> previous_last_resource;
    if (section == StructuralSection::Resources && !after.draft.resources().empty()) {
        previous_last_resource = after.draft.resources().back().id;
    }

    SystemExplorerInteraction interaction;
    auto result = interaction.create(section, after.draft, after.selection);
    if (!result.changed) {
        application.set_status(result.diagnostic, true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }

    // Copy execution profiles from the previous last resource to the newly added resource
    if (section == StructuralSection::Resources && previous_last_resource.has_value() &&
        !after.draft.resources().empty()) {
        const auto new_resource = after.draft.resources().back().id;
        for (const auto& task : after.draft.tasks()) {
            const auto inherited_wcet =
                after.draft.execution_profile(task.id, *previous_last_resource);
            if (inherited_wcet.has_value()) {
                after.draft.set_execution_profile(task.id, new_resource, *inherited_wcet);
            }
        }
    }

    for (const auto& task : after.draft.tasks()) {
        const auto exists = std::any_of(after.assignments.begin(), after.assignments.end(),
                                        [&](const auto& row) { return row.task_id == task.id; });
        if (!exists)
            after.assignments.push_back({task.id, std::nullopt});
    }
    undo_stack_.push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                             QStringLiteral("Add component")});
    return true;
}

std::optional<TaskId> QtStructuralEditController::create_task() {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value())
        return std::nullopt;
    if (edit_policy() != ProjectSystemEditPolicy::Generic) {
        application.set_status("Adapter-owned task identities are protected.", true);
        Q_EMIT bridge_.statusChanged();
        return std::nullopt;
    }

    if (!create_component(StructuralSection::Tasks))
        return std::nullopt;

    return application.structural_selection().task_id();
}

bool QtStructuralEditController::delete_selected() {
    auto& application = bridge_.application();
    if (!editing_enabled())
        return false;
    const auto selection = application.structural_selection();
    if ((selection.kind() == StructuralSelectionKind::Task ||
         selection.kind() == StructuralSelectionKind::MessageRoute ||
         selection.kind() == StructuralSelectionKind::Connection) &&
        edit_policy() == ProjectSystemEditPolicy::BoschCompatible) {
        application.set_status("This Bosch structural identity is protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(), selection};
    auto after = before;
    SystemExplorerInteraction interaction;
    if (!interaction.request_delete(after.selection, after.draft, after.assignments))
        return false;
    const auto result = interaction.confirm_delete(after.draft, after.assignments, after.selection);
    if (!result.changed)
        return false;
    undo_stack_.push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                             QStringLiteral("Delete component")});
    return true;
}

bool QtStructuralEditController::duplicate_selected() {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value()) {
        return false;
    }
    const auto selected = application.structural_selection();
    if ((selected.kind() == StructuralSelectionKind::Task ||
         selected.kind() == StructuralSelectionKind::MessageRoute ||
         selected.kind() == StructuralSelectionKind::Connection) &&
        edit_policy() != ProjectSystemEditPolicy::Generic) {
        application.set_status("Adapter-owned tasks and route structure are protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(), selected};
    auto after = before;
    SystemExplorerInteraction interaction;
    const auto result = interaction.duplicate(selected, after.draft, after.selection);
    if (!result.changed) {
        application.set_status(result.diagnostic, true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    for (const auto& task : after.draft.tasks()) {
        const auto exists = std::any_of(after.assignments.begin(), after.assignments.end(),
                                        [&](const auto& row) { return row.task_id == task.id; });
        if (!exists) {
            after.assignments.push_back({task.id, std::nullopt});
        }
    }
    undo_stack_.push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                             QStringLiteral("Duplicate component")});
    return true;
}

bool QtStructuralEditController::create_connection(TaskId source, TaskId destination,
                                                    int kind) {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value()) {
        application.set_status("Editing is not available in the current state.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    if (edit_policy() != ProjectSystemEditPolicy::Generic) {
        application.set_status("Adapter-owned route structure is protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;

    const auto link_kind = kind == 1 ? GuiConnectionKind::Logical : GuiConnectionKind::Communication;
    const auto result = create_message_route(source, destination, after.draft, after.selection, link_kind);
    if (!result.changed) {
        application.set_status(result.diagnostic, true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }

    undo_stack_.push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                             QStringLiteral("Create connection")});
    return true;
}

bool QtStructuralEditController::delete_connection(const GuiConnectionId& connection) {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value()) {
        application.set_status("Editing is not available in the current state.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    if (connection.kind != GuiConnectionKind::Communication) {
        application.set_status("Only communication routes can be deleted.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    if (edit_policy() == ProjectSystemEditPolicy::BoschCompatible) {
        application.set_status("This Bosch route identity is protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_active_project();

    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;

    const DraftMessageRouteKey key{connection.source_task_id, connection.destination_task_id};
    if (!after.draft.remove_message_route(key)) {
        application.set_status("The selected route could not be found in the draft.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    synchronize_structural_selection(after.selection, after.draft);

    undo_stack_.push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                             QStringLiteral("Delete connection")});
    return true;
}

} // namespace cpssim::qt
