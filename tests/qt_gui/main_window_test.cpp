/*** Qt Test coverage for the native workbench shell and layout identity. ***/
#include "apps/qt_gui/main_window.hpp"

#include <QDockWidget>
#include <QTabWidget>
#include <QtTest/QTest>

namespace cpssim::qt {

class QtMainWindowTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void starts_on_home_with_stable_actions();
    void exposes_required_tabs_and_docks();
    void round_trips_versioned_geometry_and_state();
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
