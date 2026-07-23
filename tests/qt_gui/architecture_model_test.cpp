/*** Verify flat QtNodes identity, cycles, placement, and draft creation. ***/
#ifdef Q_MOC_RUN
#include <QObject>
#else
#include "apps/qt_gui/architecture_model.hpp"
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"
#include "apps/qt_gui/workbench_style.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/project/project_template.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QPixmap>
#include <QUndoStack>
#include <QtTest/QTest>

#include <chrono>
#include <cmath>
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
    auto graph = GuiArchitectureGraph{
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
    graph.edges[0].connection =
        GuiConnectionPresentation{.id = {GuiConnectionKind::Logical, TaskId{1}, TaskId{2}},
                                  .label = "Logical dependency",
                                  .displayed_latency = 0,
                                  .creates_network_events = false,
                                  .protected_semantics = true};
    graph.edges[1].connection =
        GuiConnectionPresentation{.id = {GuiConnectionKind::Communication, TaskId{2}, TaskId{1}},
                                  .label = "Communication channel",
                                  .displayed_latency = 80,
                                  .creates_network_events = true,
                                  .protected_semantics = true};
    return graph;
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
    void explorer_creation_and_node_movement_use_shared_grid();
    void bosch_session_loads_paused_and_renders_six_flat_tasks();
    void addTaskAction_exists_with_object_name();
    void addTaskAction_creates_task_and_selects_with_position();
    void addTaskAction_undo_removes_and_redo_restores_with_position();
    void addTaskAction_disabled_without_editable_project();
    void addTaskAction_disabled_while_running();
    void addTaskAction_disabled_for_protected_project();
    void addTaskAction_shares_undo_history_with_system_builder();
    void allArchitectureActions_exist_with_stable_object_names();
    void architectureActions_use_scoped_shortcuts();
    void addTaskAction_single_connection_creates_one_task_per_activation();
    void duplicateAction_places_non_overlapping();
    void editAction_emits_editSelectionRequested();
    void deleteAction_state_responds_to_selection();
    void updateActionState_disabled_for_generic_running();
    void updateActionState_disabled_for_protected_adapter();
    void contextAddPosition_does_not_affect_toolbar_after_menu();

    // Stable port contract (Prompt 3).
    void isolatedTaskHasOneInOneOutPort();
    void portZeroDataIsValid();
    void invalidPortIndexReturnsInvalidData();
    void existingEdgePortIndicesAreZero();
    void fanOutUsesPortZero();
    void fanInUsesPortZero();
    void chainAllPortsZero();
    void cycleAllPortsZero();
    void selfLoopPortZero();
    void noConnectionGraph();
    void rebuildStability();
    void visibleSceneNodesHaveOneInputOneOutputPort();
    void connectionEditingIsDisabled();

    // Regression: editable draft rendering independent of session/snapshot.
    void editableDraftPopulatesGraphWithoutSession();
    void editableDraftNodeCountMatchesDraftTasks();
    void everyModelNodeHasSceneGraphicsObject();
    void addTaskCreatesVisibleSceneNode();
    void undoRemovesVisibleSceneNode();
    void redoRestoresVisibleSceneNode();
    void boschProtectedStillRendersReadOnly();

    // Style-role and scene-visibility regression tests.
    void nodeRoleStyle_returns_valid_nonzero_opacity();
    void everyTaskNodeGraphicsObject_is_visible_with_positive_opacity();
    void addTaskCreatesVisibleSceneNode_with_positive_effective_opacity();
    void undoRemovesGraphicsObject_redoRestoresWithOpacity();

    // Theme-aware style regression tests.
    void lightThemeStyle_returns_valid_light_background_dark_text();
    void darkThemeStyle_returns_valid_dark_background_light_text();
    void themeToggle_preserves_structure_and_updates_style();

    // Background-cache invalidation regression.
    void appearanceChangeInvalidatesBackgroundCache();
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
    const auto first_connections =
        model.allConnectionIds(*model.node_id_for(task_graph_node_id(TaskId{1})));
    QVERIFY(!first_connections.empty());
    QVERIFY(model.connection_for(*first_connections.begin()).has_value());
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

void QtArchitectureModelTest::explorer_creation_and_node_movement_use_shared_grid() {
    QCOMPARE(architecture_grid_step, 20.0);
    QCOMPARE(architecture_major_grid_every, 5);
    QCOMPARE(snap_architecture_position({611.0, 329.0}), QPointF(620.0, 320.0));

    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};
    QtSystemBuilderWidget builder{bridge, edits};
    QObject::connect(&builder, &QtSystemBuilderWidget::taskCreated, &view,
                     &QtArchitectureView::place_task_near_view_center);

    QVERIFY(view.findChild<QAction*>("action.architecture.addTask") != nullptr);
    QVERIFY(builder.create_component(StructuralSection::Tasks));
    const auto task_id = bridge.application().structural_selection().task_id();
    QVERIFY(task_id.has_value());
    const auto* created = find_task_layout(bridge.application().workspace().architecture, *task_id);
    QVERIFY(created != nullptr);
    QCOMPARE(std::fmod(created->position.x, static_cast<float>(architecture_grid_step)), 0.0F);
    QCOMPARE(std::fmod(created->position.y, static_cast<float>(architecture_grid_step)), 0.0F);

    const auto node_id = view.graph_model().node_id_for(task_graph_node_id(*task_id));
    QVERIFY(node_id.has_value());
    Q_EMIT view.graphics_scene().nodeMoved(*node_id, {613.0, 329.0});
    const auto* moved = find_task_layout(bridge.application().workspace().architecture, *task_id);
    QVERIFY(moved != nullptr);
    QCOMPARE(moved->position, (GuiLayoutPoint{620.0F, 320.0F}));

    auto* auto_layout = view.findChild<QAction*>("action.architecture.autoLayout");
    QVERIFY(auto_layout != nullptr);
    auto_layout->trigger();
    for (const auto& layout : bridge.application().workspace().architecture.tasks) {
        QCOMPARE(std::fmod(layout.position.x, static_cast<float>(architecture_grid_step)), 0.0F);
        QCOMPARE(std::fmod(layout.position.y, static_cast<float>(architecture_grid_step)), 0.0F);
    }
}

void QtArchitectureModelTest::draft_creation_selects_and_places_new_task() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};
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
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});
    QCOMPARE(view.graph_model().connection_count(), std::size_t{5});
    QVERIFY(!view.add_task_at({500.0, 300.0}).has_value());
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});
}

void QtArchitectureModelTest::addTaskAction_exists_with_object_name() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);
    QVERIFY(action->isEnabled());
}

void QtArchitectureModelTest::addTaskAction_creates_task_and_selects_with_position() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    const auto before = view.graph_model().node_count();

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);
    action->trigger();

    QCOMPARE(view.graph_model().node_count(), before + 1);
    const auto task_id = bridge.application().structural_selection().task_id();
    QVERIFY(task_id.has_value());

    const auto* layout =
        find_task_layout(bridge.application().workspace().architecture, *task_id);
    QVERIFY(layout != nullptr);
    QVERIFY(layout->position.x > 0.0F);
    QVERIFY(layout->position.y > 0.0F);
}

void QtArchitectureModelTest::addTaskAction_undo_removes_and_redo_restores_with_position() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    const auto before = view.graph_model().node_count();

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    action->trigger();
    const auto task_id = bridge.application().structural_selection().task_id();
    QVERIFY(task_id.has_value());
    const auto* layout =
        find_task_layout(bridge.application().workspace().architecture, *task_id);
    QVERIFY(layout != nullptr);
    const auto saved_position = layout->position;

    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(view.graph_model().node_count(), before);

    edits.undo_stack().redo();
    QCOMPARE(view.graph_model().node_count(), before + 1);
    const auto* restored =
        find_task_layout(bridge.application().workspace().architecture, *task_id);
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->position, saved_position);
}

void QtArchitectureModelTest::addTaskAction_disabled_without_editable_project() {
    auto application = std::make_unique<WorkbenchApplication>();
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);
    QVERIFY(!action->isEnabled());
}

void QtArchitectureModelTest::addTaskAction_disabled_while_running() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);
    QVERIFY(action->isEnabled());

    bridge.application().enqueue(GuiCommand::Run);
    bridge.application().update();
    Q_EMIT bridge.applicationStateChanged();

    QVERIFY(!action->isEnabled());
}

void QtArchitectureModelTest::addTaskAction_disabled_for_protected_project() {
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
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);
    QVERIFY(!action->isEnabled());
}

void QtArchitectureModelTest::addTaskAction_shares_undo_history_with_system_builder() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};
    QtSystemBuilderWidget builder{bridge, edits};

    const auto before = bridge.application().editable_system()->tasks().size();

    // Create one task through System Builder
    QVERIFY(builder.create_component(StructuralSection::Tasks));
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);

    // Create one task through Architecture Add Task action
    auto* arch_action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(arch_action != nullptr);
    arch_action->trigger();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 2);

    // Undo once removes the Architecture-created task
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);

    // Undo again removes the System-Builder-created task
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);

    // Redo once restores the System-Builder-created task
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);

    // Redo again restores the Architecture-created task
    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 2);
}

void QtArchitectureModelTest::allArchitectureActions_exist_with_stable_object_names() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Verify each expected action exists with the correct object name.
    auto* add_task = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(add_task != nullptr);

    auto* edit_action = view.findChild<QAction*>("action.architecture.edit");
    QVERIFY(edit_action != nullptr);

    auto* duplicate = view.findChild<QAction*>("action.architecture.duplicate");
    QVERIFY(duplicate != nullptr);

    auto* delete_action = view.findChild<QAction*>("action.architecture.delete");
    QVERIFY(delete_action != nullptr);

    auto* fit = view.findChild<QAction*>("action.architecture.fit");
    QVERIFY(fit != nullptr);

    auto* actual = view.findChild<QAction*>("action.architecture.actualSize");
    QVERIFY(actual != nullptr);

    auto* auto_layout = view.findChild<QAction*>("action.architecture.autoLayout");
    QVERIFY(auto_layout != nullptr);

    auto* snap = view.findChild<QAction*>("action.architecture.snapToGrid");
    QVERIFY(snap != nullptr);

    // Exactly one Add Task action with that name.
    const auto actions = view.findChildren<QAction*>("action.architecture.addTask");
    QCOMPARE(static_cast<int>(actions.size()), 1);
}

void QtArchitectureModelTest::architectureActions_use_scoped_shortcuts() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* delete_action = view.findChild<QAction*>("action.architecture.delete");
    QVERIFY(delete_action != nullptr);
    QCOMPARE(delete_action->shortcutContext(), Qt::WidgetWithChildrenShortcut);

    auto* duplicate = view.findChild<QAction*>("action.architecture.duplicate");
    QVERIFY(duplicate != nullptr);
    QCOMPARE(duplicate->shortcutContext(), Qt::WidgetWithChildrenShortcut);

    auto* fit = view.findChild<QAction*>("action.architecture.fit");
    QVERIFY(fit != nullptr);
    QCOMPARE(fit->shortcutContext(), Qt::WidgetWithChildrenShortcut);

    auto* actual = view.findChild<QAction*>("action.architecture.actualSize");
    QVERIFY(actual != nullptr);
    QCOMPARE(actual->shortcutContext(), Qt::WidgetWithChildrenShortcut);
}

void QtArchitectureModelTest::addTaskAction_single_connection_creates_one_task_per_activation() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    const auto before = view.graph_model().node_count();

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    // Trigger twice and verify each creates exactly one task.
    action->trigger();
    QCOMPARE(view.graph_model().node_count(), before + 1);

    action->trigger();
    QCOMPARE(view.graph_model().node_count(), before + 2);

    // Undo removes in reverse order.
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(view.graph_model().node_count(), before + 1);

    edits.undo_stack().undo();
    QCOMPARE(view.graph_model().node_count(), before);
}

void QtArchitectureModelTest::duplicateAction_places_non_overlapping() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // First add a task at a known position so it has a workspace layout entry.
    const auto task_id = view.add_task_at({200.0, 200.0});
    QVERIFY(task_id.has_value());

    bridge.application().structural_selection().select_task(*task_id);
    bridge.notify_structural_selection_changed();
    view.refresh();

    const auto before = bridge.application().editable_system()->tasks().size();
    QVERIFY(before >= 2); // original default task + new task

    auto* duplicate = view.findChild<QAction*>("action.architecture.duplicate");
    QVERIFY(duplicate != nullptr);
    QVERIFY(duplicate->isEnabled());
    duplicate->trigger();

    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);

    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());
    QVERIFY(*new_task != *task_id);

    // Verify non-overlapping position.
    const auto* original_layout =
        find_task_layout(bridge.application().workspace().architecture, *task_id);
    const auto* new_layout =
        find_task_layout(bridge.application().workspace().architecture, *new_task);
    QVERIFY(original_layout != nullptr);
    QVERIFY(new_layout != nullptr);

    // New position should be offset from original.
    const bool different_position = (new_layout->position.x != original_layout->position.x) ||
                                    (new_layout->position.y != original_layout->position.y);
    QVERIFY(different_position);

    // Undo/redo.
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before);

    edits.undo_stack().redo();
    QCOMPARE(bridge.application().editable_system()->tasks().size(), before + 1);
}

void QtArchitectureModelTest::editAction_emits_editSelectionRequested() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Select the first task.
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().front().id);
    bridge.notify_structural_selection_changed();
    view.refresh();

    auto* edit_action = view.findChild<QAction*>("action.architecture.edit");
    QVERIFY(edit_action != nullptr);
    QVERIFY(edit_action->isEnabled());

    bool received = false;
    QObject::connect(&view, &QtArchitectureView::editSelectionRequested,
                     [&received] { received = true; });

    edit_action->trigger();
    QVERIFY(received);
}

void QtArchitectureModelTest::deleteAction_state_responds_to_selection() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* delete_action = view.findChild<QAction*>("action.architecture.delete");
    QVERIFY(delete_action != nullptr);

    // Delete should be disabled when nothing is selected.
    QVERIFY(!delete_action->isEnabled());

    // Select a task.
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().front().id);
    bridge.notify_structural_selection_changed();
    view.refresh();

    // Now Delete should be enabled.
    QVERIFY(delete_action->isEnabled());
}

void QtArchitectureModelTest::updateActionState_disabled_for_generic_running() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Select a task first.
    bridge.application().structural_selection().select_task(
        bridge.application().editable_system()->tasks().front().id);
    bridge.notify_structural_selection_changed();
    view.refresh();

    auto* add_task = view.findChild<QAction*>("action.architecture.addTask");
    auto* duplicate = view.findChild<QAction*>("action.architecture.duplicate");
    auto* delete_action = view.findChild<QAction*>("action.architecture.delete");
    QVERIFY(add_task->isEnabled());
    QVERIFY(duplicate->isEnabled());
    QVERIFY(delete_action->isEnabled());

    // Start running.
    bridge.application().enqueue(GuiCommand::Run);
    bridge.application().update();
    Q_EMIT bridge.applicationStateChanged();
    view.refresh();

    QVERIFY(!add_task->isEnabled());
    QVERIFY(!duplicate->isEnabled());
    QVERIFY(!delete_action->isEnabled());
}

void QtArchitectureModelTest::updateActionState_disabled_for_protected_adapter() {
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
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* add_task = view.findChild<QAction*>("action.architecture.addTask");
    auto* duplicate = view.findChild<QAction*>("action.architecture.duplicate");
    auto* delete_action = view.findChild<QAction*>("action.architecture.delete");

    QVERIFY(!add_task->isEnabled());
    QVERIFY(!duplicate->isEnabled());
    QVERIFY(!delete_action->isEnabled());
    // Edit is enabled when a structural item is selected, even for protected projects
    // because viewing properties is allowed.
}

void QtArchitectureModelTest::contextAddPosition_does_not_affect_toolbar_after_menu() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    // Trigger from toolbar: should create near viewport center.
    action->trigger();
    const auto first_task = bridge.application().structural_selection().task_id();
    QVERIFY(first_task.has_value());

    // The context_add_position_ is cleared after the context menu processing.
    // Simulate what happens: set a context position, trigger the action,
    // then clear the position. The action triggers via the same path.
    // Since we can't easily simulate a context menu, we just verify that
    // triggering the toolbar action twice in a row works correctly
    // (the second trigger creates a new task, not relying on stale context).

    action->trigger();
    const auto second_task = bridge.application().structural_selection().task_id();
    QVERIFY(second_task.has_value());
    QVERIFY(*second_task != *first_task);

    // Both tasks should have valid positions.
    const auto* first_layout =
        find_task_layout(bridge.application().workspace().architecture, *first_task);
    const auto* second_layout =
        find_task_layout(bridge.application().workspace().architecture, *second_task);
    QVERIFY(first_layout != nullptr);
    QVERIFY(second_layout != nullptr);
}

// ---------------------------------------------------------------------------
// Stable port contract tests (Prompt 3)
// ---------------------------------------------------------------------------

void QtArchitectureModelTest::isolatedTaskHasOneInOneOutPort() {
    // An isolated task must have InPortCount == 1 and OutPortCount == 1 with
    // no connections required.
    const auto node_id = task_graph_node_id(TaskId{1});
    GuiArchitectureGraph graph;
    graph.nodes.push_back(
        {node_id, GuiGraphNodeKind::Task, TaskId{1}, "Isolated", {40.0F, 40.0F}, {120.0F, 60.0F}});
    graph.edges.clear();
    graph.logical_size = {200.0F, 100.0F};

    QtArchitectureGraphModel model;
    model.rebuild(graph);

    const auto adapter = model.node_id_for(node_id);
    QVERIFY(adapter.has_value());

    const auto in_count =
        model.nodeData(*adapter, QtNodes::NodeRole::InPortCount).value<QtNodes::PortCount>();
    const auto out_count =
        model.nodeData(*adapter, QtNodes::NodeRole::OutPortCount).value<QtNodes::PortCount>();

    QCOMPARE(in_count, QtNodes::PortCount{1});
    QCOMPARE(out_count, QtNodes::PortCount{1});
    QCOMPARE(model.connection_count(), std::size_t{0});
}

void QtArchitectureModelTest::portZeroDataIsValid() {
    // Port 0 must have: valid DataType with cpssim.connection identifier,
    // ConnectionPolicy::Many, visible "in" / "out" caption.
    const auto node_id = task_graph_node_id(TaskId{1});
    GuiArchitectureGraph graph;
    graph.nodes.push_back(
        {node_id, GuiGraphNodeKind::Task, TaskId{1}, "Task", {40.0F, 40.0F}, {120.0F, 60.0F}});
    QtArchitectureGraphModel model;
    model.rebuild(graph);

    const auto adapter = model.node_id_for(node_id);
    QVERIFY(adapter.has_value());

    // Check input port 0.
    const auto in_dt =
        model.portData(*adapter, QtNodes::PortType::In, 0, QtNodes::PortRole::DataType)
            .value<QtNodes::NodeDataType>();
    QVERIFY(!in_dt.id.isEmpty());
    QCOMPARE(in_dt.id, QStringLiteral("cpssim.connection"));

    const auto in_policy =
        model.portData(*adapter, QtNodes::PortType::In, 0, QtNodes::PortRole::ConnectionPolicyRole)
            .value<QtNodes::ConnectionPolicy>();
    QCOMPARE(in_policy, QtNodes::ConnectionPolicy::Many);

    const auto in_cap_vis =
        model.portData(*adapter, QtNodes::PortType::In, 0, QtNodes::PortRole::CaptionVisible)
            .toBool();
    QCOMPARE(in_cap_vis, true);

    const auto in_cap =
        model.portData(*adapter, QtNodes::PortType::In, 0, QtNodes::PortRole::Caption).toString();
    QCOMPARE(in_cap, QStringLiteral("in"));

    // Check output port 0.
    const auto out_dt =
        model.portData(*adapter, QtNodes::PortType::Out, 0, QtNodes::PortRole::DataType)
            .value<QtNodes::NodeDataType>();
    QVERIFY(!out_dt.id.isEmpty());
    QCOMPARE(out_dt.id, QStringLiteral("cpssim.connection"));

    const auto out_policy =
        model
            .portData(*adapter, QtNodes::PortType::Out, 0, QtNodes::PortRole::ConnectionPolicyRole)
            .value<QtNodes::ConnectionPolicy>();
    QCOMPARE(out_policy, QtNodes::ConnectionPolicy::Many);

    const auto out_cap_vis =
        model.portData(*adapter, QtNodes::PortType::Out, 0, QtNodes::PortRole::CaptionVisible)
            .toBool();
    QCOMPARE(out_cap_vis, true);

    const auto out_cap =
        model.portData(*adapter, QtNodes::PortType::Out, 0, QtNodes::PortRole::Caption).toString();
    QCOMPARE(out_cap, QStringLiteral("out"));
}

void QtArchitectureModelTest::invalidPortIndexReturnsInvalidData() {
    // Indices other than 0 must return invalid/empty QVariant.
    const auto node_id = task_graph_node_id(TaskId{1});
    GuiArchitectureGraph graph;
    graph.nodes.push_back(
        {node_id, GuiGraphNodeKind::Task, TaskId{1}, "Task", {40.0F, 40.0F}, {120.0F, 60.0F}});
    QtArchitectureGraphModel model;
    model.rebuild(graph);

    const auto adapter = model.node_id_for(node_id);
    QVERIFY(adapter.has_value());

    // Input port 1.
    QVERIFY(!model.portData(*adapter, QtNodes::PortType::In, 1, QtNodes::PortRole::DataType)
                 .isValid());
    QVERIFY(!model.portData(*adapter, QtNodes::PortType::In, 1, QtNodes::PortRole::ConnectionPolicyRole)
                 .isValid());

    // Output port 1.
    QVERIFY(!model.portData(*adapter, QtNodes::PortType::Out, 1, QtNodes::PortRole::DataType)
                 .isValid());
    QVERIFY(!model.portData(*adapter, QtNodes::PortType::Out, 1, QtNodes::PortRole::ConnectionPolicyRole)
                 .isValid());

    // Input port 999.
    QVERIFY(!model.portData(*adapter, QtNodes::PortType::In, 999, QtNodes::PortRole::DataType)
                 .isValid());

    // Non-existent node returns invalid regardless of port index.
    QVERIFY(!model.portData(QtNodes::InvalidNodeId, QtNodes::PortType::In, 0,
                            QtNodes::PortRole::DataType)
                 .isValid());
}

void QtArchitectureModelTest::existingEdgePortIndicesAreZero() {
    // Every existing edge must have output port index 0 and input port index 0.
    const auto first = task_graph_node_id(TaskId{1});
    const auto second = task_graph_node_id(TaskId{2});
    GuiArchitectureGraph graph;
    graph.nodes = {{first, GuiGraphNodeKind::Task, TaskId{1}, "First", {40.0F, 40.0F}, {120.0F, 60.0F}},
                   {second, GuiGraphNodeKind::Task, TaskId{2}, "Second", {280.0F, 40.0F}, {120.0F, 60.0F}}};
    graph.edges = {{{GuiGraphEdgeKind::FunctionalDependency, first, second},
                    GuiGraphEdgeKind::FunctionalDependency,
                    first,
                    second,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    GuiConnectionPresentation{
                        .id = {GuiConnectionKind::Logical, TaskId{1}, TaskId{2}},
                        .label = "dep",
                        .displayed_latency = 0,
                        .creates_network_events = false,
                        .protected_semantics = false}}};

    QtArchitectureGraphModel model;
    model.rebuild(graph);

    QCOMPARE(model.connection_count(), std::size_t{1});

    const auto all_connections = model.allConnectionIds(QtNodes::NodeId{0});
    // Collect all connections from the model.
    const auto node_ids = model.allNodeIds();
    std::unordered_set<QtNodes::ConnectionId> found;
    for (const auto nid : node_ids) {
        const auto conns = model.allConnectionIds(nid);
        found.insert(conns.begin(), conns.end());
    }
    QCOMPARE(found.size(), std::size_t{1});

    const auto conn = *found.begin();
    QCOMPARE(conn.outPortIndex, QtNodes::PortIndex{0});
    QCOMPARE(conn.inPortIndex, QtNodes::PortIndex{0});
}

void QtArchitectureModelTest::fanOutUsesPortZero() {
    // Two destinations from one source both use source output port 0.
    const auto source = task_graph_node_id(TaskId{1});
    const auto dest_a = task_graph_node_id(TaskId{2});
    const auto dest_b = task_graph_node_id(TaskId{3});
    GuiArchitectureGraph graph;
    graph.nodes = {{source, GuiGraphNodeKind::Task, TaskId{1}, "Src", {40.0F, 40.0F}, {120.0F, 60.0F}},
                   {dest_a, GuiGraphNodeKind::Task, TaskId{2}, "A", {280.0F, 40.0F}, {120.0F, 60.0F}},
                   {dest_b, GuiGraphNodeKind::Task, TaskId{3}, "B", {280.0F, 200.0F}, {120.0F, 60.0F}}};
    graph.edges = {{{GuiGraphEdgeKind::FunctionalDependency, source, dest_a},
                    GuiGraphEdgeKind::FunctionalDependency,
                    source,
                    dest_a,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt},
                   {{GuiGraphEdgeKind::FunctionalDependency, source, dest_b},
                    GuiGraphEdgeKind::FunctionalDependency,
                    source,
                    dest_b,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt}};

    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.connection_count(), std::size_t{2});

    const auto src_adapter = model.node_id_for(source);
    QVERIFY(src_adapter.has_value());

    const auto src_out_conns =
        model.connections(*src_adapter, QtNodes::PortType::Out, 0);
    QCOMPARE(src_out_conns.size(), std::size_t{2});

    for (const auto& c : src_out_conns) {
        QCOMPARE(c.outPortIndex, QtNodes::PortIndex{0});
        QCOMPARE(c.outNodeId, *src_adapter);
    }
}

void QtArchitectureModelTest::fanInUsesPortZero() {
    // Two sources into one destination both use destination input port 0.
    const auto src_a = task_graph_node_id(TaskId{1});
    const auto src_b = task_graph_node_id(TaskId{2});
    const auto dest = task_graph_node_id(TaskId{3});
    GuiArchitectureGraph graph;
    graph.nodes = {{src_a, GuiGraphNodeKind::Task, TaskId{1}, "A", {40.0F, 40.0F}, {120.0F, 60.0F}},
                   {src_b, GuiGraphNodeKind::Task, TaskId{2}, "B", {40.0F, 200.0F}, {120.0F, 60.0F}},
                   {dest, GuiGraphNodeKind::Task, TaskId{3}, "Dest", {280.0F, 120.0F}, {120.0F, 60.0F}}};
    graph.edges = {{{GuiGraphEdgeKind::FunctionalDependency, src_a, dest},
                    GuiGraphEdgeKind::FunctionalDependency,
                    src_a,
                    dest,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt},
                   {{GuiGraphEdgeKind::FunctionalDependency, src_b, dest},
                    GuiGraphEdgeKind::FunctionalDependency,
                    src_b,
                    dest,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt}};

    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.connection_count(), std::size_t{2});

    const auto dest_adapter = model.node_id_for(dest);
    QVERIFY(dest_adapter.has_value());

    const auto dest_in_conns = model.connections(*dest_adapter, QtNodes::PortType::In, 0);
    QCOMPARE(dest_in_conns.size(), std::size_t{2});

    for (const auto& c : dest_in_conns) {
        QCOMPARE(c.inPortIndex, QtNodes::PortIndex{0});
        QCOMPARE(c.inNodeId, *dest_adapter);
    }
}

void QtArchitectureModelTest::chainAllPortsZero() {
    // A chain A -> B -> C: all ports are 0.
    const auto a = task_graph_node_id(TaskId{1});
    const auto b = task_graph_node_id(TaskId{2});
    const auto c = task_graph_node_id(TaskId{3});
    GuiArchitectureGraph graph;
    graph.nodes = {{a, GuiGraphNodeKind::Task, TaskId{1}, "A", {40.0F, 40.0F}, {120.0F, 60.0F}},
                   {b, GuiGraphNodeKind::Task, TaskId{2}, "B", {280.0F, 40.0F}, {120.0F, 60.0F}},
                   {c, GuiGraphNodeKind::Task, TaskId{3}, "C", {520.0F, 40.0F}, {120.0F, 60.0F}}};
    graph.edges = {{{GuiGraphEdgeKind::FunctionalDependency, a, b},
                    GuiGraphEdgeKind::FunctionalDependency,
                    a,
                    b,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt},
                   {{GuiGraphEdgeKind::FunctionalDependency, b, c},
                    GuiGraphEdgeKind::FunctionalDependency,
                    b,
                    c,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt}};

    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.connection_count(), std::size_t{2});

    const auto all_connections = model.allConnectionIds(QtNodes::NodeId{0});
    const auto node_ids = model.allNodeIds();
    std::unordered_set<QtNodes::ConnectionId> found;
    for (const auto nid : node_ids) {
        const auto conns = model.allConnectionIds(nid);
        found.insert(conns.begin(), conns.end());
    }
    QCOMPARE(found.size(), std::size_t{2});

    for (const auto& cid : found) {
        QCOMPARE(cid.outPortIndex, QtNodes::PortIndex{0});
        QCOMPARE(cid.inPortIndex, QtNodes::PortIndex{0});
    }
}

void QtArchitectureModelTest::cycleAllPortsZero() {
    // A cycle A -> B, B -> A: all ports are 0.
    const auto graph = cyclic_graph();
    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.connection_count(), std::size_t{2});

    const auto node_ids = model.allNodeIds();
    std::unordered_set<QtNodes::ConnectionId> found;
    for (const auto nid : node_ids) {
        const auto conns = model.allConnectionIds(nid);
        found.insert(conns.begin(), conns.end());
    }
    QCOMPARE(found.size(), std::size_t{2});

    for (const auto& cid : found) {
        QCOMPARE(cid.outPortIndex, QtNodes::PortIndex{0});
        QCOMPARE(cid.inPortIndex, QtNodes::PortIndex{0});
    }
}

void QtArchitectureModelTest::selfLoopPortZero() {
    // A self-loop A -> A: uses port 0 for both.
    const auto node = task_graph_node_id(TaskId{1});
    GuiArchitectureGraph graph;
    graph.nodes = {{node, GuiGraphNodeKind::Task, TaskId{1}, "Self", {40.0F, 40.0F}, {120.0F, 60.0F}}};
    graph.edges = {{{GuiGraphEdgeKind::FunctionalDependency, node, node},
                    GuiGraphEdgeKind::FunctionalDependency,
                    node,
                    node,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt}};

    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.connection_count(), std::size_t{1});

    const auto adapter = model.node_id_for(node);
    QVERIFY(adapter.has_value());

    const auto conns = model.allConnectionIds(*adapter);
    QCOMPARE(conns.size(), std::size_t{1});

    const auto& cid = *conns.begin();
    QCOMPARE(cid.outNodeId, *adapter);
    QCOMPARE(cid.inNodeId, *adapter);
    QCOMPARE(cid.outPortIndex, QtNodes::PortIndex{0});
    QCOMPARE(cid.inPortIndex, QtNodes::PortIndex{0});
}

void QtArchitectureModelTest::noConnectionGraph() {
    // A graph with task nodes but no edges: each node still has 1/1 ports.
    const auto a = task_graph_node_id(TaskId{1});
    const auto b = task_graph_node_id(TaskId{2});
    GuiArchitectureGraph graph;
    graph.nodes = {{a, GuiGraphNodeKind::Task, TaskId{1}, "A", {40.0F, 40.0F}, {120.0F, 60.0F}},
                   {b, GuiGraphNodeKind::Task, TaskId{2}, "B", {280.0F, 40.0F}, {120.0F, 60.0F}}};
    graph.edges.clear();

    QtArchitectureGraphModel model;
    model.rebuild(graph);
    QCOMPARE(model.node_count(), std::size_t{2});
    QCOMPARE(model.connection_count(), std::size_t{0});

    for (const auto nid : model.allNodeIds()) {
        const auto in_cnt =
            model.nodeData(nid, QtNodes::NodeRole::InPortCount).value<QtNodes::PortCount>();
        const auto out_cnt =
            model.nodeData(nid, QtNodes::NodeRole::OutPortCount).value<QtNodes::PortCount>();
        QCOMPARE(in_cnt, QtNodes::PortCount{1});
        QCOMPARE(out_cnt, QtNodes::PortCount{1});
    }
}

void QtArchitectureModelTest::rebuildStability() {
    // Repeated rebuilds preserve node-ID mapping, connection count,
    // no duplicate connections, and semantic connection mapping.
    const auto graph = cyclic_graph();
    QtArchitectureGraphModel model;
    model.rebuild(graph);

    const auto first_pass_node = model.node_id_for(task_graph_node_id(TaskId{1}));
    QVERIFY(first_pass_node.has_value());
    const auto first_pass_conns = model.connection_count();

    // Rebuild again with the same graph.
    model.rebuild(graph);

    // Node mapping stable.
    QCOMPARE(model.node_id_for(task_graph_node_id(TaskId{1})), first_pass_node);

    // Connection count stable.
    QCOMPARE(model.connection_count(), first_pass_conns);

    // No duplicate connections (collect from connections_ via allConnectionIds
    // but deduplicate across endpoint nodes).
    const auto node_ids = model.allNodeIds();
    std::unordered_set<QtNodes::ConnectionId> unique;
    for (const auto nid : node_ids) {
        const auto conns = model.allConnectionIds(nid);
        for (const auto& c : conns) {
            unique.insert(c);
        }
    }
    // A connection is reported by both its endpoints, so the deduplicated set
    // must equal the total connection count.
    QCOMPARE(unique.size(), model.connection_count());

    // Semantic mapping preserved.
    const auto all_connections = model.allConnectionIds(QtNodes::NodeId{0});
    std::unordered_set<QtNodes::ConnectionId> all_conns;
    for (const auto nid : node_ids) {
        const auto cs = model.allConnectionIds(nid);
        all_conns.insert(cs.begin(), cs.end());
    }
    for (const auto& c : all_conns) {
        QVERIFY(model.connection_for(c).has_value());
    }
}

void QtArchitectureModelTest::visibleSceneNodesHaveOneInputOneOutputPort() {
    // When rendered, scene nodes must have one visible input port and one
    // visible output port with valid opacity and non-empty geometry.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto nid : node_ids) {
        const auto in_cnt =
            view.graph_model().nodeData(nid, QtNodes::NodeRole::InPortCount).value<QtNodes::PortCount>();
        const auto out_cnt =
            view.graph_model().nodeData(nid, QtNodes::NodeRole::OutPortCount).value<QtNodes::PortCount>();
        QCOMPARE(in_cnt, QtNodes::PortCount{1});
        QCOMPARE(out_cnt, QtNodes::PortCount{1});

        auto* item = view.graphics_scene().nodeGraphicsObject(nid);
        QVERIFY(item != nullptr);
        QVERIFY(item->isVisible());
        QVERIFY(item->effectiveOpacity() > 0.0);
        QVERIFY(!item->boundingRect().isEmpty());
    }
}

void QtArchitectureModelTest::connectionEditingIsDisabled() {
    // connectionPossible must return false,
    // addConnection must not mutate the model,
    // deleteConnection must return false.
    const auto graph = cyclic_graph();
    QtArchitectureGraphModel model;
    model.rebuild(graph);

    QVERIFY(!model.connectionPossible({}));

    const auto before = model.connection_count();
    model.addConnection({});
    QCOMPARE(model.connection_count(), before);

    QVERIFY(!model.deleteConnection({}));
    QCOMPARE(model.connection_count(), before);
}

void QtArchitectureModelTest::editableDraftPopulatesGraphWithoutSession() {
    // Verify that an editable draft containing tasks populates the Architecture
    // graph: after creating a generic project the draft contains tasks and the
    // graph model must not be empty.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // The draft should contain tasks from the project template.
    QVERIFY(bridge.application().editable_system().has_value());
    const auto draft_task_count = bridge.application().editable_system()->tasks().size();
    QVERIFY(draft_task_count > 0);

    // The graph model must contain the same number of task nodes.
    QCOMPARE(view.graph_model().node_count(), draft_task_count);
}

void QtArchitectureModelTest::editableDraftNodeCountMatchesDraftTasks() {
    // After adding a task, the graph model node count must match the draft
    // task count exactly (no stale or extra nodes).
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    const auto initial = bridge.application().editable_system()->tasks().size();
    QCOMPARE(view.graph_model().node_count(), initial);

    // Add one task through the controller.
    QVERIFY(edits.create_task().has_value());
    view.refresh();
    QCOMPARE(view.graph_model().node_count(), initial + 1);

    // Add a second task.
    QVERIFY(edits.create_task().has_value());
    view.refresh();
    QCOMPARE(view.graph_model().node_count(), initial + 2);
}

void QtArchitectureModelTest::everyModelNodeHasSceneGraphicsObject() {
    // After refresh and processing Qt events, every model node must have a
    // corresponding BasicGraphicsScene nodeGraphicsObject.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Add a task so we have more than just the template tasks.
    QVERIFY(edits.create_task().has_value());
    view.refresh();

    // Process events so the QtNodes scene synchronises.
    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto node_id : node_ids) {
        auto* scene_node = view.graphics_scene().nodeGraphicsObject(node_id);
        QVERIFY2(scene_node != nullptr,
                 qPrintable(QString{"nodeGraphicsObject is null for nodeId %1"}.arg(
                     static_cast<unsigned int>(node_id))));
    }
}

void QtArchitectureModelTest::addTaskCreatesVisibleSceneNode() {
    // Add Task through the toolbar action must create one visible scene node.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    const auto before = view.graph_model().node_count();
    action->trigger();
    QCOMPARE(view.graph_model().node_count(), before + 1);

    QApplication::processEvents();

    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());
    const auto new_node_id = view.graph_model().node_id_for(task_graph_node_id(*new_task));
    QVERIFY(new_node_id.has_value());

    auto* scene_node = view.graphics_scene().nodeGraphicsObject(*new_node_id);
    QVERIFY(scene_node != nullptr);
}

void QtArchitectureModelTest::undoRemovesVisibleSceneNode() {
    // Undo must remove the visible scene node created by Add Task.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    action->trigger();
    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());
    const auto new_node_id = view.graph_model().node_id_for(task_graph_node_id(*new_task));
    QVERIFY(new_node_id.has_value());

    QApplication::processEvents();
    QVERIFY(view.graphics_scene().nodeGraphicsObject(*new_node_id) != nullptr);

    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    view.refresh();
    QApplication::processEvents();

    // After undo + refresh, the scene node must be gone.
    QVERIFY(view.graphics_scene().nodeGraphicsObject(*new_node_id) == nullptr);
}

void QtArchitectureModelTest::redoRestoresVisibleSceneNode() {
    // Redo must restore the visible scene node that undo removed.
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    action->trigger();
    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());

    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    view.refresh();

    QVERIFY(edits.undo_stack().canRedo());
    edits.undo_stack().redo();
    view.refresh();

    QApplication::processEvents();

    const auto restored_node_id = view.graph_model().node_id_for(task_graph_node_id(*new_task));
    QVERIFY(restored_node_id.has_value());
    auto* scene_node = view.graphics_scene().nodeGraphicsObject(*restored_node_id);
    QVERIFY(scene_node != nullptr);
}

void QtArchitectureModelTest::boschProtectedStillRendersReadOnly() {
    // Bosch/protected projects must still render their six tasks but remain
    // structurally read-only.
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
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Must render 6 task nodes and 5 connections.
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});
    QCOMPARE(view.graph_model().connection_count(), std::size_t{5});

    // add_task_at must return nullopt for protected projects.
    QVERIFY(!view.add_task_at({500.0, 300.0}).has_value());
    QCOMPARE(view.graph_model().node_count(), std::size_t{6});

    // Verify scene objects exist.
    QApplication::processEvents();
    for (const auto node_id : view.graph_model().allNodeIds()) {
        QVERIFY(view.graphics_scene().nodeGraphicsObject(node_id) != nullptr);
    }
}

void QtArchitectureModelTest::nodeRoleStyle_returns_valid_nonzero_opacity() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto node_id : node_ids) {
        const auto style_value =
            view.graph_model().nodeData(node_id, QtNodes::NodeRole::Style);

        QVERIFY(style_value.isValid());
        QVERIFY(!style_value.isNull());

        const auto root = QJsonObject::fromVariantMap(style_value.toMap());

        QVERIFY(root.contains("NodeStyle"));

        const auto style = root.value("NodeStyle").toObject();

        QVERIFY(style.contains("Opacity"));
        QVERIFY(style.value("Opacity").toDouble() > 0.0);
    }
}

void QtArchitectureModelTest::everyTaskNodeGraphicsObject_is_visible_with_positive_opacity() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto node_id : node_ids) {
        auto* item = view.graphics_scene().nodeGraphicsObject(node_id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isVisible());
        QVERIFY(item->opacity() > 0.0);
        QVERIFY(item->effectiveOpacity() > 0.0);
        QVERIFY(!item->boundingRect().isEmpty());
    }

    // The scene itemsBoundingRect must be valid (not null/nan).
    QVERIFY(view.graphics_scene().itemsBoundingRect().isValid());
}

void QtArchitectureModelTest::addTaskCreatesVisibleSceneNode_with_positive_effective_opacity() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    action->trigger();
    QApplication::processEvents();

    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());
    const auto new_node_id = view.graph_model().node_id_for(task_graph_node_id(*new_task));
    QVERIFY(new_node_id.has_value());

    auto* item = view.graphics_scene().nodeGraphicsObject(*new_node_id);
    QVERIFY(item != nullptr);
    QVERIFY(item->opacity() > 0.0);
    QVERIFY(item->effectiveOpacity() > 0.0);
    QVERIFY(!item->boundingRect().isEmpty());
}

void QtArchitectureModelTest::undoRemovesGraphicsObject_redoRestoresWithOpacity() {
    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    auto* action = view.findChild<QAction*>("action.architecture.addTask");
    QVERIFY(action != nullptr);

    action->trigger();
    QApplication::processEvents();

    const auto new_task = bridge.application().structural_selection().task_id();
    QVERIFY(new_task.has_value());
    const auto new_node_id = view.graph_model().node_id_for(task_graph_node_id(*new_task));
    QVERIFY(new_node_id.has_value());

    // Undo removes the graphics object.
    QVERIFY(edits.undo_stack().canUndo());
    edits.undo_stack().undo();
    view.refresh();
    QApplication::processEvents();
    QVERIFY(view.graphics_scene().nodeGraphicsObject(*new_node_id) == nullptr);

    // Redo restores the graphics object with positive effective opacity.
    QVERIFY(edits.undo_stack().canRedo());
    edits.undo_stack().redo();
    view.refresh();
    QApplication::processEvents();

    auto* restored = view.graphics_scene().nodeGraphicsObject(*new_node_id);
    QVERIFY(restored != nullptr);
    QVERIFY(restored->effectiveOpacity() > 0.0);
}

void QtArchitectureModelTest::lightThemeStyle_returns_valid_light_background_dark_text() {
    const auto original = current_workbench_theme();
    apply_workbench_theme(GuiTheme::Light);

    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto node_id : node_ids) {
        const auto style_value =
            view.graph_model().nodeData(node_id, QtNodes::NodeRole::Style);
        QVERIFY(style_value.isValid());

        const auto root = QJsonObject::fromVariantMap(style_value.toMap());
        QVERIFY(root.contains("NodeStyle"));
        const auto style = root.value("NodeStyle").toObject();

        QVERIFY(style.contains("Opacity"));
        QVERIFY(style.value("Opacity").toDouble() > 0.0);

        const auto bg_color = QColor(style.value("GradientColor0").toString());
        const auto font_color = QColor(style.value("FontColor").toString());
        const auto faded_color = QColor(style.value("FontColorFaded").toString());

        // In light theme, background is light and font is dark.
        QVERIFY(bg_color.lightness() > 128);
        QVERIFY(font_color.lightness() < 128);
        QVERIFY(faded_color.lightness() < 160);

        // Adequate contrast between background and font.
        QVERIFY(std::abs(bg_color.lightness() - font_color.lightness()) > 100);

        // Graphics object must be visible.
        auto* item = view.graphics_scene().nodeGraphicsObject(node_id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isVisible());
        QVERIFY(item->effectiveOpacity() > 0.0);
    }

    apply_workbench_theme(original);
}

void QtArchitectureModelTest::darkThemeStyle_returns_valid_dark_background_light_text() {
    const auto original = current_workbench_theme();
    apply_workbench_theme(GuiTheme::Dark);

    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    const auto node_ids = view.graph_model().allNodeIds();
    QVERIFY(!node_ids.empty());

    for (const auto node_id : node_ids) {
        const auto style_value =
            view.graph_model().nodeData(node_id, QtNodes::NodeRole::Style);
        QVERIFY(style_value.isValid());

        const auto root = QJsonObject::fromVariantMap(style_value.toMap());
        QVERIFY(root.contains("NodeStyle"));
        const auto style = root.value("NodeStyle").toObject();

        QVERIFY(style.contains("Opacity"));
        QVERIFY(style.value("Opacity").toDouble() > 0.0);

        const auto bg_color = QColor(style.value("GradientColor0").toString());
        const auto font_color = QColor(style.value("FontColor").toString());
        const auto faded_color = QColor(style.value("FontColorFaded").toString());

        // In dark theme, background is dark and font is light.
        QVERIFY(bg_color.lightness() < 128);
        QVERIFY(font_color.lightness() > 128);
        QVERIFY(faded_color.lightness() > 128);

        // Adequate contrast between background and font.
        QVERIFY(std::abs(bg_color.lightness() - font_color.lightness()) > 100);

        // Graphics object must be visible.
        auto* item = view.graphics_scene().nodeGraphicsObject(node_id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isVisible());
        QVERIFY(item->effectiveOpacity() > 0.0);
    }

    apply_workbench_theme(original);
}

void QtArchitectureModelTest::themeToggle_preserves_structure_and_updates_style() {
    const auto original = current_workbench_theme();
    apply_workbench_theme(GuiTheme::Dark);

    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    QApplication::processEvents();

    // Record structural invariants in dark theme.
    const auto node_ids_dark = view.graph_model().allNodeIds();
    QVERIFY(!node_ids_dark.empty());

    // Read dark styles.
    QMap<QtNodes::NodeId, QColor> dark_bg, dark_font;
    for (const auto nid : node_ids_dark) {
        const auto sv = view.graph_model().nodeData(nid, QtNodes::NodeRole::Style);
        const auto root = QJsonObject::fromVariantMap(sv.toMap());
        const auto style = root.value("NodeStyle").toObject();
        dark_bg[nid] = QColor(style.value("GradientColor0").toString());
        dark_font[nid] = QColor(style.value("FontColor").toString());
    }

    // Switch to light theme.
    apply_workbench_theme(GuiTheme::Light);
    view.refresh();
    QApplication::processEvents();

    // Verify structure is unchanged.
    const auto node_ids_light = view.graph_model().allNodeIds();
    QCOMPARE(static_cast<int>(node_ids_light.size()), static_cast<int>(node_ids_dark.size()));

    // Verify style colors changed.
    for (const auto nid : node_ids_dark) {
        const auto sv = view.graph_model().nodeData(nid, QtNodes::NodeRole::Style);
        const auto root = QJsonObject::fromVariantMap(sv.toMap());
        const auto style = root.value("NodeStyle").toObject();

        const auto bg = QColor(style.value("GradientColor0").toString());
        const auto fg = QColor(style.value("FontColor").toString());

        // Background flipped from dark to light.
        QVERIFY(bg.lightness() > dark_bg[nid].lightness());
        // Font flipped from light to dark.
        QVERIFY(fg.lightness() < dark_font[nid].lightness());

        // Selected boundary uses Highlight palette colour.
        const auto sel = QColor(style.value("SelectedBoundaryColor").toString());
        const auto highlight = workbench_palette(GuiTheme::Light).color(QPalette::Highlight);
        // The hues should match (tolerance of about 10 lightness).
        QVERIFY(std::abs(sel.lightness() - highlight.lightness()) < 30);
    }

    // Switch back to dark and verify style changes again.
    apply_workbench_theme(GuiTheme::Dark);
    view.refresh();
    QApplication::processEvents();

    for (const auto nid : node_ids_dark) {
        const auto sv = view.graph_model().nodeData(nid, QtNodes::NodeRole::Style);
        const auto root = QJsonObject::fromVariantMap(sv.toMap());
        const auto style = root.value("NodeStyle").toObject();
        const auto bg = QColor(style.value("GradientColor0").toString());

        QVERIFY(bg.lightness() < 128);
    }

    apply_workbench_theme(original);
}

void QtArchitectureModelTest::appearanceChangeInvalidatesBackgroundCache() {
    const auto original = current_workbench_theme();

    TemporaryDirectory temporary;
    auto application = std::make_unique<WorkbenchApplication>();
    application->create_project(make_generic_project_template(temporary.path(), "project"));
    QtWorkbenchBridge bridge{std::move(application)};
    QtStructuralEditController edits{bridge};
    QtArchitectureView view{bridge, edits};

    // Show and size the view so rendering is possible.
    view.show();
    view.resize(800, 600);
    QApplication::processEvents();
    QApplication::processEvents();

    // Access the internal graphics view for transform queries.
    auto* gv = view.findChild<QtNodes::GraphicsView*>("architecture.graphicsView");
    QVERIFY(gv != nullptr);
    QVERIFY(gv->viewport()->width() > 100);

    // Record invariants in the initial (dark) theme.
    const auto before_nodes = view.graph_model().node_count();
    const auto before_connections = view.graph_model().connection_count();
    const auto before_transform = gv->viewportTransform();
    const auto before_selection = bridge.application().structural_selection();
    const auto before_undo_count = edits.undo_stack().count();

    // Switch to light theme through the production appearance path.
    apply_workbench_theme(GuiTheme::Light);
    Q_EMIT bridge.appearanceChanged();
    QApplication::processEvents();
    QApplication::processEvents();

    // Verify invariants are preserved after the theme change.
    QCOMPARE(view.graph_model().node_count(), before_nodes);
    QCOMPARE(view.graph_model().connection_count(), before_connections);
    QCOMPARE(gv->viewportTransform(), before_transform);
    QCOMPARE(bridge.application().structural_selection(), before_selection);
    QCOMPARE(edits.undo_stack().count(), before_undo_count);

    // Background cache verification: grab the viewport in both themes
    // and compare a central pixel.  The light-theme pixel must be lighter.
    const QPoint center_px(gv->viewport()->width() / 2,
                           gv->viewport()->height() / 2);

    // Dark theme grab (switch back momentarily).
    apply_workbench_theme(GuiTheme::Dark);
    Q_EMIT bridge.appearanceChanged();
    QApplication::processEvents();
    QApplication::processEvents();

    const auto dark_img = gv->viewport()->grab().toImage();
    QVERIFY(center_px.x() < dark_img.width());
    QVERIFY(center_px.y() < dark_img.height());
    const auto dark_luma = dark_img.pixelColor(center_px).lightness();

    // Light theme grab.
    apply_workbench_theme(GuiTheme::Light);
    Q_EMIT bridge.appearanceChanged();
    QApplication::processEvents();
    QApplication::processEvents();

    const auto light_img = gv->viewport()->grab().toImage();
    const auto light_luma = light_img.pixelColor(center_px).lightness();

    QVERIFY(light_luma > dark_luma);
    QVERIFY(light_luma - dark_luma > 20);

    // Restore original theme.
    apply_workbench_theme(original);
}

} // namespace cpssim::qt

QTEST_MAIN(cpssim::qt::QtArchitectureModelTest)
#include "architecture_model_test.moc"
