/*** Verify Qt analysis views consume shared presentation/completed data. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/analysis_widgets.hpp"
#include "apps/qt_gui/main_window.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QtTest/QTest>

#include <chrono>
#include <cmath>
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
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-analysis-" + suffix);
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

void finish_run(QtWorkbenchBridge& bridge) {
    QVERIFY(bridge.set_stop_tick(4));
    QVERIFY(bridge.apply_and_restart());
    bridge.run();
    bridge.process_once();
    while (bridge.run_state() == GuiRunState::Running) {
        bridge.process_once();
    }
}

} // namespace

class QtAnalysisViewsTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void shell_installs_native_analysis_views();
    void timeline_reuses_unchanged_presentation_generation();
    void results_and_plot_are_completed_run_only();
    void plot_projection_bounds_large_source_without_mutation();
};

void QtAnalysisViewsTest::shell_installs_native_analysis_views() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtMainWindow window{false};
    window.bind_workbench(&bridge);
    QVERIFY(qobject_cast<QtTimelineCanvas*>(window.central_tabs()->widget(1)) != nullptr);
    QVERIFY(qobject_cast<QtSignalsWidget*>(window.central_tabs()->widget(2)) != nullptr);
    QVERIFY(qobject_cast<QtIntegratedPlotWidget*>(window.central_tabs()->widget(3)) != nullptr);
    QVERIFY(window.findChild<QtResultsWidget*>("view.results") != nullptr);
}

void QtAnalysisViewsTest::timeline_reuses_unchanged_presentation_generation() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtTimelineCanvas timeline{bridge};
    const auto builds = timeline.build_count();
    timeline.synchronize();
    QCOMPARE(timeline.build_count(), builds);
    bridge.step();
    bridge.process_once();
    QVERIFY(timeline.build_count() > builds);
}

void QtAnalysisViewsTest::results_and_plot_are_completed_run_only() {
    TemporaryDirectory temporary;
    QtWorkbenchBridge bridge{make_application(temporary.path())};
    QtResultsWidget results{bridge};
    QtIntegratedPlotWidget plot{bridge};
    QVERIFY(!results.findChild<QPushButton*>("results.openPlot")->isEnabled());
    QCOMPARE(plot.plot_build_count(), std::uint64_t{0});

    finish_run(bridge);
    QTRY_VERIFY_WITH_TIMEOUT(bridge.application().completed_result() != nullptr, 5000);
    results.refresh();
    plot.resize(640, 360);
    plot.show();
    plot.refresh();
    QCoreApplication::processEvents();
    QVERIFY(results.findChild<QPushButton*>("results.openPlot")->isEnabled());
    QCOMPARE(results.findChild<QLabel*>("results.state")->text(),
             QString{"Completed run analysis"});
    QVERIFY(plot.plot_build_count() > 0);
    auto* canvas = plot.findChild<QWidget*>("plot.canvas");
    QVERIFY(canvas != nullptr);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, canvas->rect().center());
    QVERIFY(bridge.application().runtime_selection().tick_range().has_value());

    QSignalSpy open_spy{&results, &QtResultsWidget::openIntegratedPlotRequested};
    results.findChild<QPushButton*>("results.openPlot")->click();
    QCOMPARE(open_spy.count(), 1);
}

void QtAnalysisViewsTest::plot_projection_bounds_large_source_without_mutation() {
    GuiSignalSeries series{.descriptor = {.id = {GuiSignalScalarType::Real, "benchmark"},
                                          .path = "benchmark/value",
                                          .display_name = "Benchmark",
                                          .unit = "m",
                                          .source = "test"},
                           .samples = {}};
    series.samples.reserve(100'000);
    for (Tick tick = 0; tick < 100'000; ++tick) {
        series.samples.push_back({tick, std::sin(static_cast<double>(tick) * 0.001)});
    }
    GuiSignalModel model{{series}};
    GuiPlotDataCache cache;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(cache.update(1, model, {series.descriptor.id}, GuiPlotXAxisUnit::Ticks, {0, 99'999},
                         1920.0F));
    const auto elapsed = timer.nsecsElapsed();
    const auto* projected = cache.find(series.descriptor.id);
    QVERIFY(projected != nullptr);
    QVERIFY(projected->samples.size() <= plot_point_budget(1920.0F));
    QCOMPARE(model.series.front().samples.size(), std::size_t{100'000});
    qInfo("100k plot projection: %.3f ms, %zu points", static_cast<double>(elapsed) / 1.0e6,
          projected->samples.size());
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtAnalysisViewsTest)
#include "analysis_views_test.moc"
