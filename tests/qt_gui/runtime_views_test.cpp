/*** Verify Qt Explorer, run configuration, resources, events, and diagnostics. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/event_table_widget.hpp"
#include "apps/qt_gui/explorer_widget.hpp"
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/runtime_widgets.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QLineEdit>
#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
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
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-runtime-" + suffix);
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

class QtRuntimeViewsTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void explorer_updates_only_structural_selection();
    void run_configuration_edits_detached_stop_tick();
    void resources_select_runtime_identity();
    void canonical_model_reuses_generation_and_selects_event();
    void bound_shell_replaces_runtime_placeholders();
};

void QtRuntimeViewsTest::explorer_updates_only_structural_selection() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtSystemBuilderWidget builder{bridge};
    QtExperimentExplorerWidget explorer{bridge, builder};
    bridge.application().runtime_selection().select_experiment();
    auto* model = qobject_cast<QStandardItemModel*>(explorer.tree().model());
    QVERIFY(model != nullptr);
    const auto tasks_section = model->index(1, 0, model->index(0, 0));
    const auto first_task = model->index(0, 0, tasks_section);
    QVERIFY(first_task.isValid());
    explorer.tree().setCurrentIndex(first_task);
    QCOMPARE(bridge.application().structural_selection().kind(), StructuralSelectionKind::Task);
    QCOMPARE(bridge.application().runtime_selection().kind(), GuiSelectionKind::Experiment);
}

void QtRuntimeViewsTest::run_configuration_edits_detached_stop_tick() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtRunConfigurationWidget run{bridge};
    auto* stop = run.findChild<QLineEdit*>("runConfiguration.stopTick");
    QVERIFY(stop != nullptr);
    const auto active = bridge.application().active_session().active_plan()->stop_tick();
    stop->setText("17");
    Q_EMIT stop->editingFinished();
    QCOMPARE(bridge.application().active_session().draft().stop_tick(), Tick{17});
    QCOMPARE(bridge.application().active_session().active_plan()->stop_tick(), active);
    QVERIFY(bridge.validate_changes());
    QVERIFY(bridge.apply_and_restart());
    QCOMPARE(bridge.application().active_session().active_plan()->stop_tick(), Tick{17});
}

void QtRuntimeViewsTest::resources_select_runtime_identity() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtResourcesWidget resources{bridge};
    QVERIFY(resources.resource_model().rowCount() > 0);
    const auto* row = resources.resource_model().row_at(0);
    QVERIFY(row != nullptr);
    QCOMPARE(resources.resource_model()
                 .data(resources.resource_model().index(0, QtResourceTableModel::Utilization))
                 .toString(),
             QString{"No observations"});
    resources.table().setCurrentIndex(resources.resource_model().index(0, 0));
    Q_EMIT resources.table().clicked(resources.resource_model().index(0, 0));
    QCOMPARE(bridge.application().runtime_selection().resource_id(),
             std::optional<ResourceId>{row->id});
}

void QtRuntimeViewsTest::canonical_model_reuses_generation_and_selects_event() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QVERIFY(bridge.set_stop_tick(2));
    QVERIFY(bridge.apply_and_restart());
    bridge.run();
    bridge.process_once();
    while (bridge.run_state() == GuiRunState::Running) {
        bridge.process_once();
    }
    QtCanonicalEventsWidget events{bridge};
    QVERIFY(events.event_model().rowCount() > 0);
    const auto builds = events.event_model().row_build_count();
    events.event_model().synchronize();
    QCOMPARE(events.event_model().row_build_count(), builds);
    const auto index = events.event_model().index(0, QtCanonicalEventTableModel::Sequence);
    Q_EMIT events.table().clicked(index);
    QVERIFY(bridge.application().runtime_selection().event_sequence().has_value());
    for (int row = 0; row < events.event_model().rowCount(); ++row) {
        const auto* event = events.event_model().row_at(row);
        if (event->cause.has_value()) {
            Q_EMIT events.table().clicked(
                events.event_model().index(row, QtCanonicalEventTableModel::Cause));
            QCOMPARE(bridge.application().runtime_selection().event_sequence(), event->cause);
            break;
        }
    }
}

void QtRuntimeViewsTest::bound_shell_replaces_runtime_placeholders() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtMainWindow window{false};
    window.bind_workbench(&bridge);
    QVERIFY(window.findChild<QtExperimentExplorerWidget*>("view.explorer") != nullptr);
    QVERIFY(window.findChild<QtRunConfigurationWidget*>("view.runConfiguration") != nullptr);
    QVERIFY(window.findChild<QtRuntimeInspectorWidget*>("view.runtimeInspector") != nullptr);
    QVERIFY(window.findChild<QtResourcesWidget*>("view.resources") != nullptr);
    QVERIFY(window.findChild<QtCanonicalEventsWidget*>("view.canonicalEvents") != nullptr);
    QVERIFY(window.findChild<QtDiagnosticsWidget*>("view.diagnostics") != nullptr);
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtRuntimeViewsTest)
#include "runtime_views_test.moc"
