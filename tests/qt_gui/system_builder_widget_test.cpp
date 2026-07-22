/*** Verify domain-backed Qt System Builder editing and undo/redo. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
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
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-builder-" + suffix);
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

class QtSystemBuilderWidgetTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void selection_uses_reusable_editor_pages();
    void creation_and_deletion_are_domain_undo_commands();
    void structured_validation_is_shown_on_selected_page();
    void main_window_reuses_undo_actions();
    void builder_is_a_scrollable_property_editor_without_component_library();
    void task_page_edits_assignment_and_wcets_with_undo();
};

void QtSystemBuilderWidgetTest::selection_uses_reusable_editor_pages() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};
    const auto task = bridge.application().editable_system()->tasks().front().id;
    bridge.application().structural_selection().select_task(task);
    bridge.notify_structural_selection_changed();
    auto* name = builder.findChild<QLineEdit*>("systemBuilder.taskName");
    QVERIFY(name != nullptr);
    QCOMPARE(name->text(),
             QString::fromStdString(bridge.application().editable_system()->tasks().front().name));
}

void QtSystemBuilderWidgetTest::creation_and_deletion_are_domain_undo_commands() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};
    const auto active_task_count = bridge.application().active_session().config().tasks().size();
    const auto before = bridge.application().editable_system()->tasks().size();

    QVERIFY(builder.create_component(StructuralSection::Tasks));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(bridge.application().structural_selection().task_id().has_value());
    QCOMPARE(bridge.application().active_session().config().tasks().size(), active_task_count);
    QVERIFY(edits.undo_stack().canUndo());

    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(builder.delete_selected(false) == false);
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(builder.delete_selected(true));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
}

void QtSystemBuilderWidgetTest::structured_validation_is_shown_on_selected_page() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};
    auto draft = *bridge.application().editable_system();
    const auto task = draft.tasks().front();
    draft.set_task_timing(0, {.period = 0, .deadline = task.deadline, .offset = task.offset},
                          task.priority);
    StructuralSelection selection;
    selection.select_task(task.id);
    bridge.restore_draft(std::move(draft), bridge.application().run_assignments(), selection);
    auto* diagnostic = builder.findChild<QLabel*>("systemBuilder.taskDiagnostic");
    QVERIFY(diagnostic != nullptr);
    QVERIFY(!diagnostic->text().isEmpty());
}

void QtSystemBuilderWidgetTest::main_window_reuses_undo_actions() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtMainWindow window{false};
    window.bind_workbench(&bridge);
    auto* builder = window.findChild<QtSystemBuilderWidget*>("view.systemBuilder");
    auto* undo = window.findChild<QAction*>("action.undo");
    auto* redo = window.findChild<QAction*>("action.redo");
    QVERIFY(builder != nullptr);
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);
    QVERIFY(builder->create_component(StructuralSection::Resources));
    QVERIFY(undo->isEnabled());
    undo->trigger();
    QVERIFY(redo->isEnabled());
}

void QtSystemBuilderWidgetTest::
    builder_is_a_scrollable_property_editor_without_component_library() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};
    QVERIFY(builder.findChild<QScrollArea*>("systemBuilder.scrollArea") != nullptr);
    QVERIFY(builder.findChild<QListWidget*>("systemBuilder.componentLibrary") == nullptr);
    QVERIFY(builder.findChild<QObject*>("splitter.systemBuilder") == nullptr);
}

void QtSystemBuilderWidgetTest::task_page_edits_assignment_and_wcets_with_undo() {
    TemporaryDirectory temporary;
    auto application = make_application(temporary.path());
    auto draft = *application->editable_system();
    const auto task_id = draft.tasks().front().id;
    const auto resource_id = draft.add_resource();
    draft.set_resource_name(draft.resources().size() - 1, "Accelerator");
    StructuralSelection selection;
    selection.select_task(task_id);
    application->restore_system_draft(std::move(draft), application->run_assignments(), selection);
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtSystemBuilderWidget builder{bridge, edits};

    auto* assignment = builder.findChild<QComboBox*>("systemBuilder.taskAssignment");
    auto* status = builder.findChild<QLabel*>("systemBuilder.assignmentStatus");
    auto* profile_button =
        builder.findChild<QPushButton*>("systemBuilder.editExecutionProfile");
    QVERIFY(assignment != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(profile_button != nullptr);
    QCOMPARE(assignment->count(),
             static_cast<int>(bridge.application().editable_system()->resources().size()) + 1);

    assignment->setCurrentIndex(assignment->findData(static_cast<qulonglong>(resource_id.value())));
    QCOMPARE(bridge.application().run_assignments().front().resource_id,
             std::optional<ResourceId>{resource_id});
    QCOMPARE(status->text(), QString{"Missing WCET profile"});
    QVERIFY(!bridge.application().editable_system()->execution_profile(task_id, resource_id));

    // Verify the profile button reflects incomplete state
    QVERIFY(!profile_button->property("profileComplete").toBool());

    // Test the edit model directly (simulates what the dialog does)
    QtTaskExecutionProfileEditModel edit_model;
    edit_model.load(task_id, *bridge.application().editable_system());
    QCOMPARE(edit_model.rowCount(),
             static_cast<int>(bridge.application().editable_system()->resources().size()));

    // The new resource has no profile yet — it's marked inaccessible
    int profile_row = -1;
    for (int row = 0; row < edit_model.rowCount(); ++row) {
        if (edit_model.row_at(row)->resource_id == resource_id) {
            profile_row = row;
            break;
        }
    }
    QVERIFY(profile_row >= 0);
    QVERIFY(!edit_model.row_at(profile_row)->accessible);
    QVERIFY(!edit_model.row_at(profile_row)->execution_time.has_value());

    // Make the resource accessible and set WCET
    QVERIFY(edit_model.setData(
        edit_model.index(profile_row, QtTaskExecutionProfileEditModel::Accessible),
        Qt::Checked, Qt::CheckStateRole));
    QVERIFY(!edit_model.complete()); // accessible but no WCET

    QVERIFY(edit_model.setData(
        edit_model.index(profile_row, QtTaskExecutionProfileEditModel::ExecutionTime),
        QString{"9"}));
    QVERIFY(edit_model.complete()); // now accessible and has WCET

    // Apply the mutation through the draft directly (as the dialog Done button does)
    {
        auto& draft_mut = *bridge.application().editable_system();
        draft_mut.set_execution_profile(task_id, resource_id, Tick{9});
    }
    QCOMPARE(bridge.application().editable_system()->execution_profile(task_id, resource_id),
             std::optional<Tick>{9});
    // Trigger a refresh so the status label updates
    bridge.notify_structural_selection_changed();
    QCOMPARE(status->text(), QString{"Valid"});

    // Undo should revert
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QVERIFY(!bridge.application().editable_system()->execution_profile(task_id, resource_id));
    QCOMPARE(bridge.application().run_assignments().front().resource_id,
             std::optional<ResourceId>{ResourceId{1}});
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtSystemBuilderWidgetTest)
#include "system_builder_widget_test.moc"
