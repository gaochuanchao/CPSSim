/*** Verify Qt timer scheduling and GUI-thread workbench publication. ***/
#include "apps/qt_gui/workbench_bridge.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <chrono>
#include <memory>
#include <utility>

namespace cpssim::qt {
namespace {

ExperimentConfig make_config() {
    return ExperimentConfig{std::chrono::nanoseconds{100'000},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            {ResourceSpec{ResourceId{1}, "cpu"}},
                            {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{10, 10, 0}, 1}},
                            {TaskResourceProfile{TaskId{1}, ResourceId{1}, 1}}};
}

std::unique_ptr<GuiSimulationSession> make_session(Tick stop_tick) {
    auto session = std::make_unique<GuiSimulationSession>(make_config(), stop_tick);
    if (!session->set_draft_assignment(TaskId{1}, ResourceId{1}) || !session->apply_draft()) {
        return {};
    }
    return session;
}

std::unique_ptr<WorkbenchApplication> make_application(Tick stop_tick) {
    return std::make_unique<WorkbenchApplication>(make_session(stop_tick));
}

} // namespace

class QtWorkbenchBridgeTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void live_timer_exists_only_while_running();
    void fast_mode_uses_cooperative_continuations();
    void finished_run_publishes_finalized_result();
    void run_plan_files_round_trip_through_bridge();
};

void QtWorkbenchBridgeTest::live_timer_exists_only_while_running() {
    QtWorkbenchBridge bridge{make_application(1'000)};
    QCOMPARE(bridge.run_state(), GuiRunState::Paused);
    QVERIFY(!bridge.live_timer_active());

    bridge.run();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.run_state(), GuiRunState::Running, 500);
    QTRY_VERIFY_WITH_TIMEOUT(bridge.live_timer_active(), 500);
    bridge.pause();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.run_state(), GuiRunState::Paused, 500);
    QVERIFY(!bridge.live_timer_active());

    bridge.reset();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.progress().current_tick, Tick{0}, 500);
    QVERIFY(!bridge.live_timer_active());
}

void QtWorkbenchBridgeTest::fast_mode_uses_cooperative_continuations() {
    auto application = make_application(100'000);
    application->workspace().run_mode = GuiRunMode::Fast;
    application->workspace().fast_batch_unit = GuiFastBatchUnit::Events;
    application->workspace().fast_event_batch_size = 1;
    QtWorkbenchBridge bridge{std::move(application)};

    bridge.run();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.run_state(), GuiRunState::Running, 500);
    QVERIFY(!bridge.live_timer_active());
    bridge.pause();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.run_state(), GuiRunState::Paused, 500);
    QVERIFY(!bridge.live_timer_active());
}

void QtWorkbenchBridgeTest::finished_run_publishes_finalized_result() {
    QtWorkbenchBridge bridge{make_application(20)};
    QSignalSpy completed{&bridge, &QtWorkbenchBridge::completedResultChanged};
    bridge.run();
    QTRY_COMPARE_WITH_TIMEOUT(bridge.run_state(), GuiRunState::Finished, 2'000);
    QVERIFY(!bridge.live_timer_active());
    QTRY_VERIFY_WITH_TIMEOUT(bridge.application().completed_result() != nullptr, 2'000);
    QVERIFY(completed.count() >= 1);
}

void QtWorkbenchBridgeTest::run_plan_files_round_trip_through_bridge() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QtWorkbenchBridge bridge{make_application(100)};
    QVERIFY(bridge.set_stop_tick(20));
    const std::filesystem::path path =
        std::filesystem::path{temporary.path().toStdString()} / "plan.json";
    QVERIFY(bridge.save_run_plan(path));
    QVERIFY(bridge.set_stop_tick(30));
    QCOMPARE(bridge.application().active_session().draft().stop_tick(), Tick{30});
    QVERIFY(bridge.load_run_plan(path));
    QCOMPARE(bridge.application().active_session().draft().stop_tick(), Tick{20});
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtWorkbenchBridgeTest)
#include "workbench_bridge_test.moc"
