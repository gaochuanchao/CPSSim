/*** Render CPSSim tasks and semantic connections through flat QtNodes. ***/
#include "apps/qt_gui/architecture_view.hpp"

#include "apps/qt_gui/architecture_node_painter.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"
#include "apps/qt_gui/workbench_style.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/project/system_edit_policy.hpp"
#include "cpssim/gui/presentation_model.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace cpssim::qt {
namespace {

class QtArchitectureGraphicsView final : public QtNodes::GraphicsView {
  public:
    using QtNodes::GraphicsView::GraphicsView;

  protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override {
        QtNodes::GraphicsView::drawBackground(painter, rect);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);
        auto minor = palette().color(QPalette::Mid);
        auto major = palette().color(QPalette::Highlight);
        minor.setAlpha(55);
        major.setAlpha(80);
        const auto first_x =
            std::floor(rect.left() / architecture_grid_step) * architecture_grid_step;
        const auto first_y =
            std::floor(rect.top() / architecture_grid_step) * architecture_grid_step;
        const auto major_step = architecture_grid_step * architecture_major_grid_every;
        for (qreal x = first_x; x <= rect.right(); x += architecture_grid_step) {
            const auto major_line =
                std::abs(std::remainder(x, major_step)) < std::numeric_limits<qreal>::epsilon();
            painter->setPen(major_line ? major : minor);
            painter->drawLine(QLineF{x, rect.top(), x, rect.bottom()});
        }
        for (qreal y = first_y; y <= rect.bottom(); y += architecture_grid_step) {
            const auto major_line =
                std::abs(std::remainder(y, major_step)) < std::numeric_limits<qreal>::epsilon();
            painter->setPen(major_line ? major : minor);
            painter->drawLine(QLineF{rect.left(), y, rect.right(), y});
        }
        painter->restore();
    }
};

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

QtArchitectureView::QtArchitectureView(QtWorkbenchBridge& bridge,
                                     QtStructuralEditController& edits,
                                     QWidget* parent)
    : QWidget(parent), bridge_{bridge}, edits_{edits}, model_{this},
      scene_{std::make_unique<QtNodes::BasicGraphicsScene>(model_)},
      view_{std::make_unique<QtArchitectureGraphicsView>(scene_.get())} {
    setObjectName("view.architecture");
    view_->setObjectName("architecture.graphicsView");
    view_->setScaleRange(0.1, 4.0);
    scene_->setNodePainter(std::make_unique<QtArchitectureNodePainter>());
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* toolbar = new QToolBar("Architecture", this);
    toolbar->setObjectName("toolbar.architecture");
    auto* fit = toolbar->addAction("Fit");
    fit->setObjectName("action.architecture.fit");
    auto* actual_size = toolbar->addAction("100%");
    actual_size->setObjectName("action.architecture.actualSize");
    auto* layout_action = toolbar->addAction("Auto Layout");
    layout_action->setObjectName("action.architecture.autoLayout");
    auto* snap = toolbar->addAction("Snap to Grid");
    snap->setObjectName("action.architecture.snapToGrid");
    snap->setCheckable(true);
    snap->setChecked(true);
    layout->addWidget(toolbar);
    layout->addWidget(view_.get());

    connect(fit, &QAction::triggered, view_.get(), &QtNodes::GraphicsView::zoomFitAll);
    connect(actual_size, &QAction::triggered, view_.get(), &QGraphicsView::resetTransform);
    connect(layout_action, &QAction::triggered, this, &QtArchitectureView::auto_layout);
    connect(snap, &QAction::toggled, this, [this](bool enabled) { snap_to_grid_ = enabled; });
    connect(scene_.get(), &QtNodes::BasicGraphicsScene::nodeClicked, this,
            &QtArchitectureView::select_node);
    connect(scene_.get(), &QtNodes::BasicGraphicsScene::nodeSelected, this,
            &QtArchitectureView::select_node);
    connect(scene_.get(), &QGraphicsScene::selectionChanged, this,
            &QtArchitectureView::select_scene_item);
    connect(scene_.get(), &QtNodes::BasicGraphicsScene::nodeMoved, this,
            &QtArchitectureView::snap_node_position);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { refresh(); });
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtArchitectureView::synchronize_scene_selection);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::resourceHighlightChanged, this,
            &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::workspaceChanged, this, &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::appearanceChanged, this, &QtArchitectureView::refresh);
    model_.set_position_changed([this](GuiGraphNodeId entity, QPointF position) {
        persist_node_position(entity, position);
    });
    refresh();
}

void QtArchitectureView::select_scene_item() {
    for (auto* item : scene_->selectedItems()) {
        if (item->type() != QtNodes::ConnectionGraphicsObject::Type) {
            continue;
        }
        const auto* connection = static_cast<QtNodes::ConnectionGraphicsObject*>(item);
        const auto id = model_.connection_for(connection->connectionId());
        if (id.has_value()) {
            bridge_.application().structural_selection().select_connection(*id);
            bridge_.notify_structural_selection_changed();
        }
        return;
    }
}

QtArchitectureView::~QtArchitectureView() = default;

void QtArchitectureView::refresh() {
    auto& application = bridge_.application();
    if (!application.has_active_session() || application.presentation_snapshot() == nullptr) {
        model_.rebuild({});
        return;
    }
    for (auto& task : application.workspace().architecture.tasks) {
        const auto snapped = snap_architecture_position({task.position.x, task.position.y});
        task.position = {static_cast<float>(snapped.x()), static_cast<float>(snapped.y())};
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
    const auto tasks = build_task_node_presentations(presentation, current_workbench_theme(),
                                                     bridge_.resource_highlight());
    model_.rebuild(flat_graph(std::move(graph), application.workspace().architecture), tasks);
    for (const auto node_id : model_.allNodeIds()) {
        const auto* task = model_.task_presentation(node_id);
        if (task == nullptr) {
            continue;
        }
        if (auto* item = scene_->nodeGraphicsObject(node_id); item != nullptr) {
            item->setToolTip(task->assignment_valid
                                 ? QString{"Assigned to %1 with execution time %2 ticks"}
                                       .arg(task->resource_name)
                                       .arg(*task->execution_time)
                                 : QString{"%1: assignment is incomplete or inaccessible"}.arg(
                                       task->resource_name));
        }
    }
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

    const auto task_id = edits_.create_task();
    if (!task_id.has_value()) {
        return std::nullopt;
    }

    const auto position = next_available_node_position(scene_position, QSizeF{180.0, 86.0},
                                                       model_.occupied_rectangles());
    const auto snapped = snap_architecture_position(position);
    set_task_layout_position(application.workspace().architecture, *task_id,
                             {static_cast<float>(snapped.x()), static_cast<float>(snapped.y())});
    application.validate_system_draft();
    bridge_.notify_structural_selection_changed();
    refresh();
    return task_id;
}

void QtArchitectureView::place_task_near_view_center(TaskId task_id) {
    const auto node_id = model_.node_id_for(task_graph_node_id(task_id));
    if (!node_id.has_value()) {
        return;
    }
    const auto center = view_->mapToScene(view_->viewport()->rect().center());
    const auto size = model_.nodeData(*node_id, QtNodes::NodeRole::Size).toSizeF();
    const auto position =
        next_available_node_position(snap_architecture_position(center), size,
                                     model_.occupied_rectangles(task_graph_node_id(task_id)));
    static_cast<void>(model_.setNodeData(*node_id, QtNodes::NodeRole::Position,
                                         snap_architecture_position(position)));
    bridge_.workspace_settings_changed();
}

void QtArchitectureView::snap_node_position(QtNodes::NodeId node_id, QPointF position) {
    if (!snap_to_grid_) {
        bridge_.workspace_settings_changed();
        return;
    }
    const auto snapped = snap_architecture_position(position);
    if (snapped != position) {
        static_cast<void>(model_.setNodeData(node_id, QtNodes::NodeRole::Position, snapped));
    }
    bridge_.workspace_settings_changed();
}

void QtArchitectureView::auto_layout() {
    auto ids = model_.allNodeIds();
    std::vector<QtNodes::NodeId> ordered(ids.begin(), ids.end());
    std::sort(ordered.begin(), ordered.end());
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const auto position = QPointF{40.0 + static_cast<qreal>(index % 4) * 240.0,
                                      40.0 + static_cast<qreal>(index / 4) * 140.0};
        static_cast<void>(model_.setNodeData(ordered[index], QtNodes::NodeRole::Position,
                                             snap_architecture_position(position)));
    }
    bridge_.workspace_settings_changed();
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
