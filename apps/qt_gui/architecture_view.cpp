/*** Render CPSSim tasks and semantic connections through flat QtNodes. ***/
#include "apps/qt_gui/architecture_view.hpp"

#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/gui/presentation_model.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <QHBoxLayout>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace cpssim::qt {
namespace {

GuiArchitectureGraph flat_graph(GuiArchitectureGraph graph,
                                const GuiArchitectureWorkspace& workspace) {
    std::size_t index = 0;
    for (auto& node : graph.nodes) {
        if (node.kind != GuiGraphNodeKind::Task) {
            continue;
        }
        if (find_task_layout(workspace, std::get<TaskId>(node.entity)) == nullptr) {
            node.position = {.x = 40.0F + static_cast<float>(index % 4) * 240.0F,
                             .y = 40.0F + static_cast<float>(index / 4) * 140.0F};
        }
        ++index;
    }
    return graph;
}

} // namespace

QtArchitectureView::QtArchitectureView(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, model_{this},
      scene_{std::make_unique<QtNodes::BasicGraphicsScene>(model_)},
      view_{std::make_unique<QtNodes::GraphicsView>(scene_.get())} {
    setObjectName("view.architecture");
    view_->setObjectName("architecture.graphicsView");
    view_->setScaleRange(0.1, 4.0);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* toolbar = new QToolBar("Architecture", this);
    toolbar->setObjectName("toolbar.architecture");
    auto* add_task = toolbar->addAction("Add Task");
    add_task->setObjectName("action.architecture.addTask");
    auto* fit = toolbar->addAction("Fit");
    fit->setObjectName("action.architecture.fit");
    layout->addWidget(toolbar);
    layout->addWidget(view_.get());

    connect(add_task, &QAction::triggered, this, &QtArchitectureView::add_task_at_view_center);
    connect(fit, &QAction::triggered, view_.get(), &QtNodes::GraphicsView::zoomFitAll);
    connect(scene_.get(), &QtNodes::BasicGraphicsScene::nodeClicked, this,
            &QtArchitectureView::select_node);
    connect(scene_.get(), &QtNodes::BasicGraphicsScene::nodeSelected, this,
            &QtArchitectureView::select_node);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { refresh(); });
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtArchitectureView::synchronize_scene_selection);
    model_.set_position_changed([this](GuiGraphNodeId entity, QPointF position) {
        persist_node_position(entity, position);
    });
    refresh();
}

QtArchitectureView::~QtArchitectureView() = default;

void QtArchitectureView::refresh() {
    auto& application = bridge_.application();
    if (!application.has_active_session() || application.presentation_snapshot() == nullptr) {
        model_.rebuild({});
        return;
    }
    auto presentation = application.presentation_snapshot()->experiment;
    if (application.editable_system().has_value()) {
        presentation = build_draft_experiment_presentation(*application.editable_system(),
                                                           application.run_assignments());
    }
    const auto bosch = application.has_active_project() &&
                       application.active_project().metadata().scenario_kind == "bosch";
    const auto dependencies =
        bosch ? bosch_functional_dependencies() : std::vector<GuiFunctionalDependency>{};
    auto graph = build_architecture_graph(presentation, dependencies, bosch,
                                          &application.workspace().architecture);
    model_.rebuild(flat_graph(std::move(graph), application.workspace().architecture));
    synchronize_scene_selection();
}

void QtArchitectureView::select_node(QtNodes::NodeId node_id) {
    const auto entity = model_.entity_for(node_id);
    if (!entity.has_value() || entity->kind != GuiGraphNodeKind::Task) {
        return;
    }
    bridge_.application().structural_selection().select_task(TaskId{entity->entity_value});
    bridge_.notify_structural_selection_changed();
}

void QtArchitectureView::persist_node_position(GuiGraphNodeId entity, QPointF position) {
    if (entity.kind != GuiGraphNodeKind::Task) {
        return;
    }
    set_task_layout_position(bridge_.application().workspace().architecture,
                             TaskId{entity.entity_value},
                             {static_cast<float>(position.x()), static_cast<float>(position.y())});
}

std::optional<TaskId> QtArchitectureView::add_task_at(QPointF scene_position) {
    auto& application = bridge_.application();
    if (!application.editable_system().has_value() ||
        application.run_state() == GuiRunState::Running) {
        return std::nullopt;
    }
    auto result = application.explorer_interaction().create(StructuralSection::Tasks,
                                                            *application.editable_system(),
                                                            application.structural_selection());
    if (!result.changed) {
        application.set_status(result.diagnostic.empty() ? "A task could not be created."
                                                         : std::move(result.diagnostic),
                               true);
        return std::nullopt;
    }
    application.synchronize_system_assignments();
    const auto task_id = application.structural_selection().task_id();
    if (!task_id.has_value()) {
        return std::nullopt;
    }
    const auto position = next_available_node_position(scene_position, QSizeF{180.0, 86.0},
                                                       model_.occupied_rectangles());
    set_task_layout_position(application.workspace().architecture, *task_id,
                             {static_cast<float>(position.x()), static_cast<float>(position.y())});
    application.validate_system_draft();
    bridge_.notify_structural_selection_changed();
    refresh();
    return task_id;
}

void QtArchitectureView::add_task_at_view_center() {
    const auto center = view_->mapToScene(view_->viewport()->rect().center());
    static_cast<void>(add_task_at(center));
}

void QtArchitectureView::synchronize_scene_selection() {
    scene_->clearSelection();
    const auto task = bridge_.application().structural_selection().task_id();
    if (!task.has_value()) {
        return;
    }
    const auto node_id = model_.node_id_for(task_graph_node_id(*task));
    if (node_id.has_value()) {
        if (auto* item = scene_->nodeGraphicsObject(*node_id); item != nullptr) {
            item->setSelected(true);
        }
    }
}

} // namespace cpssim::qt
