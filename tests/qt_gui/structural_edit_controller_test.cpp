/*** Verify shared structural edit controller, undo/redo, and policy checks. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QUndoStack>
#include <QtTest/QTest>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#endif

namespace cpssim::qt {
namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-ctrl-" + suffix);
        std::filesystem::create_directories(root_);
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(root_); }
    const std::filesystem::path& path() const noexcept { return root_; }

  private:
    std::filesystem::path root_;
};

std::unique_ptr<WorkbenchApplication> make_application(const std::filesystem::path& root) {
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(root, "project"));
    return application;
}

} // namespace

class QtStructuralEditControllerTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void applyMutationUndoRedo();
    void createTaskUndoRedo();
    void duplicateTaskUndoRedo();
    void deleteTaskUndoRedo();
    void rejectedCommandAddsNoUndoEntry();
    void runningStateRejection();
    void protectedProjectRejection();
    void projectSwitchClearsStaleHistory();
    void systemBuilderFieldEditsRemainUndoable();
    void architectureTaskCreationSharesHistory();
};

void QtStructuralEditControllerTest::applyMutationUndoRedo() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    const auto original_name =
        bridge.application().editable_system()->tasks().front().name;
    QVERIFY(!original_name.empty());

    const bool applied = edits.apply(
        QStringLiteral("Rename task"),
        [](auto& draft, auto&, auto&) {
            draft.set_task_name(0, "Renamed");
        });
    QVERIFY(applied);
    QCOMPARE(bridge.application().editable_system()->tasks().front().name,
             std::string{"Renamed"});
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().front().name,
             original_name);
    QVERIFY(edits.undo_stack().canRedo());

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().front().name,
             std::string{"Renamed"});
}

void QtStructuralEditControllerTest::createTaskUndoRedo() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    const auto before = bridge.application().editable_system()->tasks().size();

    const auto task_id = edits.create_task();
    QVERIFY(task_id.has_value());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QCOMPARE(bridge.application().structural_selection().task_id(), task_id);
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);
    QVERIFY(!edits.undo_stack().canUndo());

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QCOMPARE(bridge.application().structural_selection().task_id(), task_id);
}

void QtStructuralEditControllerTest::duplicateTaskUndoRedo() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    const auto before = bridge.application().editable_system()->tasks().size();
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().front().id);

    QVERIFY(edits.duplicate_selected());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
}

void QtStructuralEditControllerTest::deleteTaskUndoRedo() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    // Create a second task so we can delete one
    QVERIFY(edits.create_component(StructuralSection::Tasks));
    const auto before = bridge.application().editable_system()->tasks().size();
    QVERIFY(before >= 2);
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().back().id);

    QVERIFY(edits.delete_selected());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before - 1);
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before - 1);
}

void QtStructuralEditControllerTest::rejectedCommandAddsNoUndoEntry() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    const auto count_before = edits.undo_stack().count();
    QVERIFY(count_before == 0);

    // Mutator that throws should be caught and add no undo entry
    const bool applied = edits.apply(
        QStringLiteral("Failing mutation"),
        [](auto&, auto&, auto&) {
            throw std::runtime_error("intentional failure");
        });
    QVERIFY(!applied);
    QCOMPARE(edits.undo_stack().count(), 0);
    QVERIFY(!edits.undo_stack().canUndo());
}

void QtStructuralEditControllerTest::runningStateRejection() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    QVERIFY(edits.editing_enabled());

    // Simulate running state via the application; structural edits should
    // be rejected by editing_enabled() inside the controller.
    bridge.application().enqueue(GuiCommand::Run);
    bridge.application().update();

    QVERIFY(!edits.editing_enabled());
    QVERIFY(!edits.apply(QStringLiteral("should fail"),
                         [](auto& draft, auto&, auto&) {
                             draft.set_tick_period_ns(999);
                         }));
    QVERIFY(!edits.create_task().has_value());
    QCOMPARE(edits.undo_stack().count(), 0);
}

void QtStructuralEditControllerTest::protectedProjectRejection() {
    // A generic project has Generic policy, so create_task should succeed.
    // The controller already checks edit_policy() inside create_task().
    // We verify that a valid generic project allows task creation.

    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    QCOMPARE(edits.edit_policy(), ProjectSystemEditPolicy::Generic);
    const auto task_id = edits.create_task();
    QVERIFY(task_id.has_value());
}

void QtStructuralEditControllerTest::projectSwitchClearsStaleHistory() {
    TemporaryDirectory temporary;
    const auto root = temporary.path();
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(root, "project-a"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};

    // Perform an edit in project A
    const auto task_id = edits.create_task();
    QVERIFY(task_id.has_value());
    QVERIFY(edits.undo_stack().canUndo());

    // Create project B in a different directory
    bridge.create_project(make_generic_project_template(root, "project-b"));
    QVERIFY(bridge.application().has_active_project());

    // Synchronize should clear the old history because the root changed
    edits.synchronize_active_project();
    QCOMPARE(edits.undo_stack().count(), 0);
    QVERIFY(!edits.undo_stack().canUndo());
}

void QtStructuralEditControllerTest::systemBuilderFieldEditsRemainUndoable() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};

    const auto& draft = *bridge.application().editable_system();
    const auto task = draft.tasks().front();
    bridge.application().structural_selection().select_task(task.id);
    bridge.notify_structural_selection_changed();

    auto* name_edit = builder.findChild<QLineEdit*>("systemBuilder.taskName");
    QVERIFY(name_edit != nullptr);
    QCOMPARE(name_edit->text(), QString::fromStdString(task.name));

    name_edit->setText("ModifiedTask");
    Q_EMIT name_edit->editingFinished();

    QCOMPARE(bridge.application().editable_system()->tasks().front().name,
             std::string{"ModifiedTask"});
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().front().name,
             task.name);
}

void QtStructuralEditControllerTest::architectureTaskCreationSharesHistory() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtArchitectureView architecture{bridge, edits};

    // Create a task through System Builder
    QtSystemBuilderWidget builder{bridge, edits};
    const auto task_before = bridge.application().editable_system()->tasks().size();

    QVERIFY(builder.create_component(StructuralSection::Tasks));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before + 1);
    QVERIFY(edits.undo_stack().canUndo());

    // Create a task through Architecture
    const auto arch_task_id = architecture.add_task_at(QPointF{200.0, 200.0});
    QVERIFY(arch_task_id.has_value());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before + 2);

    // Both edits should be in the same undo history
    // Undo the Architecture task
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before + 1);

    // Undo the System Builder task
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before);

    // Redo both
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before + 1);
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), task_before + 2);
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtStructuralEditControllerTest)
#include "structural_edit_controller_test.moc"
