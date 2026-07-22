/*** Verify flat QtNodes identity, cycles, placement, and draft creation. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/architecture_model.hpp"
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/project/project_template.hpp"

#include <QtNodes/BasicGraphicsScene>

#include <QtTest/QTest>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
        root_ = std::filesystem::temp_directory_path() / ("cpssim-qt-graph-" + suffix);
        std::filesystem::create_directories(root_);
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(root_); }
    const std::filesystem::path& path() const noexcept { return root_; }

  private:
    std::filesystem::path root_;
};

GuiArchitectureGraph cyclic_graph() {
    const auto first = task_graph_node_id(TaskId{1});
    const auto second = task_graph_node_id(TaskId{2});
    const auto resource = resource_graph_node_id(ResourceId{1});
    return {
        .nodes =
            {{first, GuiGraphNodeKind::Task, TaskId{1}, "First", {40.0F, 40.0F}, {120.0F, 60.0F}},
             {second,
              GuiGraphNodeKind::Task,
              TaskId{2},
              "Second",
              {280.0F, 40.0F},
              {120.0F, 60.0F}},
             {resource,
              GuiGraphNodeKind::Resource,
              ResourceId{1},
              "CPU",
              {0.0F, 0.0F},
              {500.0F, 200.0F}}},
        .edges = {{{GuiGraphEdgeKind::FunctionalDependency, first, second},
                   GuiGraphEdgeKind::FunctionalDependency,
                   first,
                   second,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt},
                  {{GuiGraphEdgeKind::MessageRoute, second, first},
                   GuiGraphEdgeKind::MessageRoute,
                   second,
                   first,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt},
                  {{GuiGraphEdgeKind::Assignment, first, resource},
                   GuiGraphEdgeKind::Assignment,
                   first,
                   resource,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt}},
        .logical_size = {500.0F, 200.0F}};
}

std::filesystem::path make_trajectory(const std::filesystem::path& parent) {
    const auto root = parent / "trajectory-source";
    std::filesystem::create_directory(root);
    for (const auto* file : {"feedforward_sequence_0.csv", "feedforward_sequence_1.csv",
                             "x_position_track.csv", "y_position_track.csv"}) {
        std::ofstream{root / file} << "0\n0\n0\n";
    }
    std::ofstream{root / "time_vector.csv"} << "0\n0.0001\n0.0002\n";
    std::ofstream{root / "velocity.csv"} << "10\n10\n10\n";
    return root;
}

std::filesystem::path fmu_library() {
    const auto* value = std::getenv("CPSSIM_G1_BOSCH_FMU_LIBRARY");
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

} // namespace

class QtArchitectureModelTest final : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void strong_ids_are_mapped_without_truncation();
    void flat_model_accepts_cycles_and_filters_assignment_structure();
    void occupied_creation_uses_deterministic_offset();
    void draft_creation_selects_and_places_new_task();
    void bosch_session_loads_paused_and_renders_six_flat_tasks();
};

void QtArchitectureModelTest::strong_ids_are_mapped_without_truncation() {
    QtNodeIdMap map;
    const GuiGraphNodeId high{GuiGraphNodeKind::Task, (std::uint64_t{1} << 40U) + 7U};
    const GuiGraphNodeId low{GuiGraphNodeKind::Task, 7U};
    const auto high_adapter = map.adapter_id(high);
    const auto low_adapter = map.adapter_id(low);
    QVERIFY(high_adapter != low_adapter);
    QCOMPARE(map.entity_id(high_adapter), std::optional<GuiGraphNodeId>{high});
    QCOMPARE(map.adapter_id(high), high_adapter);
}

void QtArchitectureModelTest::flat_model_accepts_cycles_and_filters_assignment_structure() {
    QtArchitectureGraphModel model;
    const auto graph = cyclic_graph();
    model.rebuild(graph);
    QCOMPARE(model.node_count(), std::size_t{2});
    QCOMPARE(model.connection_count(), std::size_t{2});
    QVERIFY(model.loopsEnabled());
    const auto first_before = model.node_id_for(task_graph_node_id(TaskId{1}));
    model.rebuild(graph);
    QCOMPARE(model.node_id_for(task_graph_node_id(TaskId{1})), first_before);
}

void QtArchitectureModelTest::occupied_creation_uses_deterministic_offset() {
    const std::vector<QRectF> occupied{QRectF{QPointF{100.0, 100.0}, QSizeF{180.0, 86.0}}};
    const auto first = next_available_node_position({100.0, 100.0}, {180.0, 86.0}, occupied, 24.0);
    const auto second = next_available_node_position({100.0, 100.0}, {180.0, 86.0}, occupied, 24.0);
    QCOMPARE(first, second);
    QVERIFY(first != QPointF(100.0, 100.0));
    QCOMPARE(first.x() - 100.0, first.y() - 100.0);
}

void QtArchitectureModelTest::draft_creation_selects_and_places_new_task() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtArchitectureView view{bridge};
    const auto before = view.graph_model().node_count();
    const auto original = view.graph_model().node_id_for(task_graph_node_id(TaskId{1}));
    QVERIFY(original.has_value());
    Q_EMIT view.graphics_scene().nodeClicked(*original);
    QCOMPARE(bridge.application().structural_selection().task_id(),
             std::optional<TaskId>{TaskId{1}});
    const auto created = view.add_task_at({600.0, 320.0});
    QVERIFY(created.has_value());
    QCOMPARE(view.graph_model().node_count(), before + 1);
    QCOMPARE(bridge.application().structural_selection().task_id(), created);
    const auto* layout = find_task_layout(bridge.application().workspace().architecture, *created);
    QVERIFY(layout != nullptr);
    QVERIFY(layout->position.x >= 600.0F);
    QVERIFY(layout->position.y >= 320.0F);

    const auto project_file = bridge.application().active_project().root() / "project.json";
    bridge.application().save_project();
    WorkbenchApplication reopened;
    reopened.open_project(project_file);
    const auto* restored = find_task_layout(reopened.workspace().architecture, *created);
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->position, layout->position);
}

void QtArchitectureModelTest::bosch_session_loads_paused_and_renders_six_flat_tasks() {
    TemporaryDirectory temporary;
    const auto repository =
        std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    auto project =
        create_bosch_project({.parent_directory = temporary.path(),
                              .name = "bosch",
                              .trajectory_directory = make_trajectory(temporary.path()),
                              .scenario = BoschReferenceScenario::Dedicated,
                              .stop_tick = 2,
                              .reference_root = repository / "experiments/bosch_v10_reference",
                              .shared_library = fmu_library()});
    auto application = std::make_unique<WorkbenchApplication>(std::move(project));
    QtWorkbenchBridge bridge{std::move(application)};
    QtArchitectureView view{bridge};
    QCOMPARE(bridge.run_state(), GuiRunState::Paused);
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});
    QCOMPARE(view.graph_model().connection_count(), std::size_t{5});
    QVERIFY(!view.add_task_at({500.0, 300.0}).has_value());
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtArchitectureModelTest)
#include "architecture_model_test.moc"
