/*** Verify resource identity, mapping edits, delegates, and synchronized selection. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/resource_assignment_model.hpp"
#include "apps/qt_gui/workbench_style.hpp"

#include "cpssim/application/project/project_template.hpp"
#include "cpssim/gui/presentation_model.hpp"

#include <QtNodes/BasicGraphicsScene>

#include <QComboBox>
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
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-assignment-" + suffix);
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

class QtResourceAssignmentModelTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void colors_are_stable_from_resource_identity();
    void table_edit_mutates_draft_and_updates_accessibility();
    void delegate_lists_resources_and_unassigned();
    void table_and_canvas_share_structural_selection();
};

void QtResourceAssignmentModelTest::colors_are_stable_from_resource_identity() {
    const auto first = resource_accent_color(ResourceId{42}, GuiTheme::Dark);
    QCOMPARE(resource_accent_color(ResourceId{42}, GuiTheme::Dark), first);
    QVERIFY(resource_accent_color(ResourceId{43}, GuiTheme::Dark) != first);
    QVERIFY(resource_accent_color(ResourceId{42}, GuiTheme::Light) != first);

    TemporaryDirectory temporary;
    auto application = make_application(temporary.path());
    const auto resource_id = application->editable_system()->resources().front().id;
    const auto project_file = application->active_project().root() / "project.json";
    application->save_project();
    WorkbenchApplication reopened;
    reopened.open_project(project_file);
    QCOMPARE(
        resource_accent_color(reopened.editable_system()->resources().front().id, GuiTheme::Dark),
        resource_accent_color(resource_id, GuiTheme::Dark));
}

void QtResourceAssignmentModelTest::table_edit_mutates_draft_and_updates_accessibility() {
    TemporaryDirectory temporary;
    auto application = make_application(temporary.path());
    const auto task_id = application->editable_system()->tasks().front().id;
    const auto new_resource = application->editable_system()->add_resource();
    application->editable_system()->set_resource_name(
        application->editable_system()->resources().size() - 1, "Accelerator");
    application->editable_system()->set_execution_profile(task_id, new_resource, 7);
    application->synchronize_system_assignments();
    QtWorkbenchBridge bridge{std::move(application)};
    QtResourceAssignmentModel model{bridge};
    QCOMPARE(model.rowCount(), 1);
    const auto resource_index = model.index(0, QtResourceAssignmentModel::Resource);
    QVERIFY(model.setData(resource_index,
                          QVariant::fromValue(static_cast<qulonglong>(new_resource.value()))));
    QCOMPARE(bridge.application().run_assignments().front().resource_id,
             std::optional<ResourceId>{new_resource});
    const auto* row = model.row_at(0);
    QVERIFY(row->accessible);
    QCOMPARE(row->execution_time, std::optional<Tick>{7});
    QCOMPARE(row->status, QString{"Valid"});
    QCOMPARE(bridge.application().active_session().active_plan()->assignments().front().resource_id,
             ResourceId{1});

    QVERIFY(model.setData(resource_index, QVariant{}));
    QVERIFY(!bridge.application().run_assignments().front().resource_id.has_value());
    QCOMPARE(model.row_at(0)->resource_name, QString{"Unassigned"});
    const auto presentation = build_draft_experiment_presentation(
        *bridge.application().editable_system(), bridge.application().run_assignments());
    const auto node = build_task_node_presentations(presentation, GuiTheme::Dark).front();
    QCOMPARE(node.resource_name, QString{"Unassigned"});
    QVERIFY(!node.assignment_valid);
}

void QtResourceAssignmentModelTest::delegate_lists_resources_and_unassigned() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtResourceAssignmentModel model{bridge};
    QtResourceAssignmentDelegate delegate;
    QWidget parent;
    const auto index = model.index(0, QtResourceAssignmentModel::Resource);
    std::unique_ptr<QWidget> editor{delegate.createEditor(&parent, {}, index)};
    auto* combo = qobject_cast<QComboBox*>(editor.get());
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), static_cast<int>(model.resources().size()) + 1);
    QCOMPARE(combo->itemText(0), QString{"Unassigned"});
}

void QtResourceAssignmentModelTest::table_and_canvas_share_structural_selection() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtArchitectureView architecture{bridge};
    QtResourceAssignmentsWidget assignments{bridge};
    const auto task_id = assignments.assignment_model().row_at(0)->task_id;
    assignments.table().selectRow(0);
    QCOMPARE(bridge.application().structural_selection().task_id(), std::optional<TaskId>{task_id});

    const auto node_id = architecture.graph_model().node_id_for(task_graph_node_id(task_id));
    QVERIFY(node_id.has_value());
    Q_EMIT architecture.graphics_scene().nodeClicked(*node_id);
    QCOMPARE(assignments.table().currentIndex().row(), 0);

    const auto presentation = build_draft_experiment_presentation(
        *bridge.application().editable_system(), bridge.application().run_assignments());
    const auto nodes = build_task_node_presentations(presentation, GuiTheme::Dark);
    QCOMPARE(
        nodes.front().resource_name,
        QString::fromStdString(bridge.application().editable_system()->resources().front().name));
    QVERIFY(nodes.front().assignment_valid);
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtResourceAssignmentModelTest)
#include "resource_assignment_model_test.moc"
