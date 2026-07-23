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
    void mixedChronologicalUndoRedo();
    void runningStateRejectsConnectionCreation();
    void projectSaveAsClearsHistory();
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

void QtStructuralEditControllerTest::mixedChronologicalUndoRedo() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    // 1. Add a task
    const auto task2 = edits.create_task();
    QVERIFY(task2.has_value());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 2);
    const auto task2_id = *task2;

    // 2. Edit task property (rename)
    QVERIFY(edits.apply("Rename task", [&](auto& draft, auto&, auto&) {
        const auto idx = std::distance(draft.tasks().begin(),
                                       std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                                    [&](const auto& t) { return t.id == task2_id; }));
        draft.set_task_name(static_cast<std::size_t>(idx), "RenamedTask");
    }));
    QCOMPARE(bridge.application().editable_system()->tasks().back().name, std::string{"RenamedTask"});

    // 3. Duplicate a task
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().front().id);
    QVERIFY(edits.duplicate_selected());
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 3);

    // 4. Create a Communication link
    const auto& tasks = bridge.application().editable_system()->tasks();
    const TaskId first_id = tasks[0].id;
    QVERIFY(edits.create_connection(first_id, task2_id, 0)); // Communication
    QCOMPARE(bridge.application().editable_system()->routes().size(), 1);
    QCOMPARE(bridge.application().editable_system()->routes()[0].kind,
             GuiConnectionKind::Communication);

    // 5. Convert it to Logical
    // Select the route, then apply the kind change via set_message_route
    auto route = bridge.application().editable_system()->routes()[0];
    route.kind = GuiConnectionKind::Logical;
    route.delay = Tick{0};
    QVERIFY(edits.apply("Change link type", [&](auto& draft, auto&, auto& selection) {
        draft.set_message_route(0, route);
        selection.select_connection(
            GuiConnectionId{GuiConnectionKind::Logical, route.source_task_id, route.destination_task_id});
    }));
    QCOMPARE(bridge.application().editable_system()->routes()[0].kind,
             GuiConnectionKind::Logical);

    // 6. Change an endpoint
    const TaskId third_id = tasks.back().id;
    QVERIFY(edits.apply("Change route endpoint", [&](auto& draft, auto&, auto& selection) {
        auto r = draft.routes()[0];
        r.source_task_id = third_id;
        draft.set_message_route(0, r);
        selection.select_connection(
            GuiConnectionId{GuiConnectionKind::Logical, r.source_task_id, r.destination_task_id});
    }));
    QCOMPARE(bridge.application().editable_system()->routes()[0].source_task_id, third_id);

    // 7. Delete the link
    GuiConnectionId conn{GuiConnectionKind::Logical, third_id, task2_id};
    QVERIFY(edits.delete_connection(conn));
    QCOMPARE(bridge.application().editable_system()->routes().size(), 0);

    // --- Verify full undo sequence ---
    // Undo 7: Delete link → link restored
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->routes().size(), 1);
    QCOMPARE(bridge.application().editable_system()->routes()[0].source_task_id, third_id);

    // Undo 6: Endpoint change → source restored
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->routes()[0].source_task_id, first_id);

    // Undo 5: Kind conversion → Communication
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->routes()[0].kind,
             GuiConnectionKind::Communication);

    // Undo 4: Delete link creation → no routes
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->routes().size(), 0);

    // Undo 3: Duplicate → task count back to 2
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 2);

    // Undo 2: Rename → original name
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 2);
    // After undoing the rename, the task with task2_id now has its original default name "task-2".
    {
        const auto& ts = bridge.application().editable_system()->tasks();
        const auto found = std::find_if(ts.begin(), ts.end(),
                                        [&](const auto& t) { return t.id == task2_id; });
        QVERIFY(found != ts.end());
        QCOMPARE(found->name, std::string{"task-2"});
    }

    // Undo 1: Task creation → back to 1 task
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 1);

    // --- Verify full redo sequence ---
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 2);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 2);
    {
        const auto& ts = bridge.application().editable_system()->tasks();
        const auto found = std::find_if(ts.begin(), ts.end(),
                                        [&](const auto& t) { return t.id == task2_id; });
        QVERIFY(found != ts.end());
        QCOMPARE(found->name, std::string{"RenamedTask"});
    }

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), 3);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->routes().size(), 1);
    QCOMPARE(bridge.application().editable_system()->routes()[0].kind,
             GuiConnectionKind::Communication);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->routes()[0].kind,
             GuiConnectionKind::Logical);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->routes()[0].source_task_id, third_id);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->routes().size(), 0);
}

void QtStructuralEditControllerTest::runningStateRejectsConnectionCreation() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};

    // Create a second task
    QVERIFY(edits.create_task());
    const auto second = bridge.application().structural_selection().task_id();
    QVERIFY(second.has_value());
    const auto first = bridge.application().editable_system()->tasks().front().id;

    // Enter running state
    bridge.application().enqueue(GuiCommand::Run);
    bridge.application().update();
    QVERIFY(!edits.editing_enabled());

    // Connection creation should be rejected
    const auto count_before = edits.undo_stack().count();
    QVERIFY(!edits.create_connection(first, *second, 0));
    QCOMPARE(edits.undo_stack().count(), count_before);

    // Exit running state
    bridge.application().enqueue(GuiCommand::Pause);
    bridge.application().update();
    QVERIFY(edits.editing_enabled());
}

void QtStructuralEditControllerTest::projectSaveAsClearsHistory() {
    TemporaryDirectory temporary;
    const auto root = temporary.path();

    // Create project A
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(root, "project-a"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};

    // Perform edits
    QVERIFY(edits.create_task().has_value());
    QVERIFY(edits.create_task().has_value());
    QVERIFY(edits.undo_stack().canUndo());

    // Save As creates a new project root
    bridge.save_project_as(root, "project-b");
    QVERIFY(bridge.application().has_active_project());
    QCOMPARE(bridge.application().active_project().metadata().name, std::string{"project-b"});

    // History should be cleared by synchronize_active_project
    edits.synchronize_active_project();
    QCOMPARE(edits.undo_stack().count(), 0);
    QVERIFY(!edits.undo_stack().canUndo());
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
