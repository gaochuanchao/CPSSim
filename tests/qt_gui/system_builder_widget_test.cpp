/*** Verify domain-backed Qt System Builder editing and undo/redo. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
};

void QtSystemBuilderWidgetTest::selection_uses_reusable_editor_pages() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtSystemBuilderWidget builder{bridge};
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
    QtSystemBuilderWidget builder{bridge};
    const auto active_task_count = bridge.application().active_session().config().tasks().size();
    const auto before = bridge.application().editable_system()->tasks().size();

    QVERIFY(builder.create_component(StructuralSection::Tasks));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(bridge.application().structural_selection().task_id().has_value());
    QCOMPARE(bridge.application().active_session().config().tasks().size(), active_task_count);
    QVERIFY(builder.undo_stack().canUndo());

    builder.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);
    builder.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(builder.delete_selected(false) == false);
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
    QVERIFY(builder.delete_selected(true));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);
    builder.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
}

void QtSystemBuilderWidgetTest::structured_validation_is_shown_on_selected_page() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtSystemBuilderWidget builder{bridge};
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
    QtSystemBuilderWidget builder{bridge};
    QVERIFY(builder.findChild<QScrollArea*>("systemBuilder.scrollArea") != nullptr);
    QVERIFY(builder.findChild<QListWidget*>("systemBuilder.componentLibrary") == nullptr);
    QVERIFY(builder.findChild<QObject*>("splitter.systemBuilder") == nullptr);
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtSystemBuilderWidgetTest)
#include "system_builder_widget_test.moc"
