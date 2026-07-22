/*** Qt Test coverage for the native workbench shell and layout identity. ***/
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/workbench_application.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace cpssim::qt {

class QtMainWindowTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void starts_on_home_with_stable_actions();
    void exposes_required_tabs_and_docks();
    void round_trips_versioned_geometry_and_state();
    void project_workflow_and_recent_history_are_native();
    void execution_controls_and_global_theme_are_independent();
};

void QtMainWindowTest::starts_on_home_with_stable_actions() {
    QtMainWindow window{false};
    QVERIFY(window.home_is_active());
    QCOMPARE(window.run_action()->objectName(), QString{"action.run"});
    QCOMPARE(window.pause_action()->objectName(), QString{"action.pause"});
    QCOMPARE(window.reset_action()->objectName(), QString{"action.reset"});
    QCOMPARE(window.step_action()->objectName(), QString{"action.step"});
    QVERIFY(window.findChild<QWidget*>("home.createProject") != nullptr);
    QVERIFY(window.findChild<QWidget*>("home.openProject") != nullptr);
    QVERIFY(window.findChild<QWidget*>("home.boschProject") != nullptr);
    QVERIFY(window.findChild<QAction*>("action.theme.dark")->isEnabled());
    QVERIFY(window.findChild<QAction*>("action.theme.light")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("action.resetLayout")->isEnabled());
    for (auto* dock : window.findChildren<QDockWidget*>()) {
        QVERIFY(!dock->toggleViewAction()->isEnabled());
    }
}

void QtMainWindowTest::project_workflow_and_recent_history_are_native() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const std::filesystem::path root{temporary.path().toStdString()};
    QtMainWindow window{false};
    auto application = std::make_unique<WorkbenchApplication>(WorkbenchApplicationPaths{
        .projects_directory = root, .preferences_file = root / "preferences.json"});
    auto* bridge = new QtWorkbenchBridge(std::move(application), &window);
    window.bind_workbench(bridge);

    QVERIFY(window.create_generic_project_at(root, "native-project"));
    QVERIFY(!window.home_is_active());
    QVERIFY(std::filesystem::exists(root / "native-project/project.json"));
    QVERIFY(window.save_project_as_to(root, "native-copy"));
    QCOMPARE(bridge->application().active_project().metadata().name, std::string{"native-copy"});
    QVERIFY(std::filesystem::exists(root / "native-copy/project.json"));
    QVERIFY(!bridge->application().recent_projects().entries().empty());
    auto* recent = window.findChild<QWidget*>("home.recentProjects");
    QVERIFY(recent != nullptr);
    QVERIFY(recent->maximumWidth() <= 720);
    QVERIFY(window.findChild<QPushButton*>("recent.open")->maximumWidth() <= 84);

    bridge->close_project();
    QVERIFY(window.home_is_active());
    QVERIFY(window.open_project_path(root / "native-project/project.json"));
    QCOMPARE(bridge->application().active_project().metadata().name, std::string{"native-project"});
}

void QtMainWindowTest::execution_controls_and_global_theme_are_independent() {
    QtMainWindow window{false};
    auto* bridge = new QtWorkbenchBridge(std::make_unique<WorkbenchApplication>(), &window);
    window.bind_workbench(bridge);
    auto* mode = window.findChild<QComboBox*>("runMode");
    auto* unit = window.findChild<QComboBox*>("fastBatchUnit");
    auto* size = window.findChild<QLineEdit*>("fastBatchSize");
    QVERIFY(mode != nullptr);
    QVERIFY(unit != nullptr);
    QVERIFY(size != nullptr);
    mode->setCurrentIndex(1);
    unit->setCurrentIndex(1);
    size->setText("37");
    Q_EMIT size->editingFinished();
    QCOMPARE(bridge->application().workspace().run_mode, GuiRunMode::Fast);
    QCOMPARE(bridge->application().workspace().fast_batch_unit, GuiFastBatchUnit::Ticks);
    QCOMPARE(bridge->application().workspace().fast_tick_batch_size, std::uint64_t{37});
    const auto project_theme = bridge->application().workspace().theme;
    window.findChild<QAction*>("action.theme.light")->trigger();
    QCOMPARE(QtAppearancePreferences{}.theme(), GuiTheme::Light);
    QCOMPARE(bridge->application().workspace().theme, project_theme);
    QtAppearancePreferences{}.set_theme(GuiTheme::Dark);
}

void QtMainWindowTest::exposes_required_tabs_and_docks() {
    QtMainWindow window{false};
    window.show_workbench();
    window.show();
    QCoreApplication::processEvents();
    QCOMPARE(window.central_tabs()->count(), 4);
    QCOMPARE(window.central_tabs()->tabText(0), QString{"Architecture"});
    QCOMPARE(window.central_tabs()->tabText(3), QString{"Integrated Plot"});
    for (const auto* name : {"dock.explorer", "dock.systemBuilder", "dock.runConfiguration",
                             "dock.runtimeInspector", "dock.resourceAssignments", "dock.resources",
                             "dock.canonicalEvents", "dock.results", "dock.diagnostics"}) {
        auto* dock = window.findChild<QDockWidget*>(name);
        QVERIFY2(dock != nullptr, name);
        QVERIFY(dock->isVisibleTo(&window));
    }
}

void QtMainWindowTest::round_trips_versioned_geometry_and_state() {
    QtMainWindow source{false};
    source.show_workbench();
    source.resize(1200, 760);
    auto* diagnostics = source.findChild<QDockWidget*>("dock.diagnostics");
    QVERIFY(diagnostics != nullptr);
    diagnostics->hide();

    QtMainWindow restored{false};
    restored.show_workbench();
    QVERIFY(restored.restore_workbench_layout(source.save_workbench_geometry(),
                                              source.save_workbench_state()));
    QVERIFY(!restored.findChild<QDockWidget*>("dock.diagnostics")->isVisible());
    QVERIFY(!restored.restore_workbench_layout({}, QByteArray{"not a Qt state"}));
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtMainWindowTest)
#include "main_window_test.moc"
