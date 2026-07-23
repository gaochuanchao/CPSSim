/*** Shared structural edit controller owning the single QUndoStack. ***/
#pragma once

#include <QObject>
#include <QUndoStack>

#include <filesystem>
#include <functional>
#include <optional>

namespace cpssim {

class EditableSystemDraft;
class StructuralSelection;
struct DraftTaskAssignment;
enum class StructuralSection : int;
enum class ProjectSystemEditPolicy : int;
class TaskId;
struct GuiConnectionId;

} // namespace cpssim

namespace cpssim::qt {

class QtWorkbenchBridge;

/***
 * Owns the single structural QUndoStack and all undoable domain mutations.
 *
 * System Builder field edits, component creation, duplication, deletion,
 * and Architecture task creation all go through this controller so that
 * undo/redo operates on one chronological history.
 *
 * The controller does not depend on QWidget, QtArchitectureView,
 * QtSystemBuilderWidget, QDockWidget, or QtNodes graphics classes.
 ***/
class QtStructuralEditController final : public QObject {
    Q_OBJECT

  public:
    using DraftMutator =
        std::function<void(EditableSystemDraft&, std::vector<DraftTaskAssignment>&,
                           StructuralSelection&)>;

    explicit QtStructuralEditController(QtWorkbenchBridge& bridge,
                                        QObject* parent = nullptr);

    QUndoStack& undo_stack() noexcept;

    bool editing_enabled() const;
    ProjectSystemEditPolicy edit_policy() const;

    // Generic mutation with before/after snapshot and undo entry.
    bool apply(const QString& command_text, DraftMutator mutator);

    // Create one component in the given section; emits taskCreated via bridge.
    bool create_component(StructuralSection section);

    // Create a task and return its new ID; used by Architecture and System
    // Builder.
    std::optional<TaskId> create_task();

    bool duplicate_selected();
    bool delete_selected();

    // Create one task link between explicit task endpoints with the given kind.
    // Returns false and sets the application diagnostic on failure.
    // kind: 0=Communication, 1=Logical (int avoids GUI header in MOC)
    bool create_connection(TaskId source, TaskId destination, int kind = 0);

    // Delete the communication route identified by the given connection id.
    // Returns false and sets the application diagnostic on failure.
    bool delete_connection(const GuiConnectionId& connection);

    // Clear undo history only when the active project root genuinely changes.
    void synchronize_active_project();

  private:
    QtWorkbenchBridge& bridge_;
    QUndoStack undo_stack_;
    std::optional<std::filesystem::path> undo_project_root_;
};

} // namespace cpssim::qt
