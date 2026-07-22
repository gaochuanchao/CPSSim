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

#include <QAction>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
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
    using ContextMenuHandler =
        std::function<void(const QPoint& viewport_position, const QPointF& scene_position)>;

    void set_context_menu_handler(ContextMenuHandler handler) {
        context_menu_handler_ = std::move(handler);
    }

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

    void contextMenuEvent(QContextMenuEvent* event) override {
        if (context_menu_handler_) {
            context_menu_handler_(event->pos(), mapToScene(event->pos()));
            event->accept();
            return;
        }
        QtNodes::GraphicsView::contextMenuEvent(event);
    }

  private:
    ContextMenuHandler context_menu_handler_;
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

    auto* toolbar = new QToolBar("Architecture", this);
    toolbar->setObjectName("toolbar.architecture");
    build_actions();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(toolbar);
    toolbar->addAction(fit_action_);
    toolbar->addAction(actual_size_action_);
    toolbar->addAction(auto_layout_action_);
    toolbar->addAction(snap_action_);
    toolbar->addAction(add_task_action_);
    layout->addWidget(view_.get());

    auto* graphics_view = static_cast<QtArchitectureGraphicsView*>(view_.get());
    graphics_view->set_context_menu_handler(
        [this](const QPoint& vp, const QPointF& sp) { show_context_menu(vp, sp); });

    connect(fit_action_, &QAction::triggered, view_.get(), &QtNodes::GraphicsView::zoomFitAll);
    connect(actual_size_action_, &QAction::triggered, view_.get(),
            &QGraphicsView::resetTransform);
    connect(auto_layout_action_, &QAction::triggered, this, &QtArchitectureView::auto_layout);
    connect(snap_action_, &QAction::toggled, this,
            [this](bool enabled) { snap_to_grid_ = enabled; });
    connect(add_task_action_, &QAction::triggered, this,
            &QtArchitectureView::trigger_add_task);
    connect(duplicate_action_, &QAction::triggered, this,
            &QtArchitectureView::duplicate_selected);
    connect(delete_action_, &QAction::triggered, this,
            &QtArchitectureView::delete_selected_with_confirmation);
    connect(edit_action_, &QAction::triggered, this,
            &QtArchitectureView::request_edit_selected);

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
    update_action_state();
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

    // Prefer the editable draft; fall back to the runtime presentation
    // snapshot only when no draft is open.
    std::optional<ExperimentPresentationSnapshot> presentation;
    if (application.editable_system().has_value()) {
        presentation = build_draft_experiment_presentation(*application.editable_system(),
                                                           application.run_assignments());
    } else if (application.has_active_session() &&
               application.presentation_snapshot() != nullptr) {
        presentation = application.presentation_snapshot()->experiment;
    }

    if (!presentation.has_value()) {
        model_.rebuild({});
        update_action_state();
        return;
    }

    for (auto& task : application.workspace().architecture.tasks) {
        const auto snapped = snap_architecture_position({task.position.x, task.position.y});
        task.position = {static_cast<float>(snapped.x()), static_cast<float>(snapped.y())};
    }
    const auto bosch = application.has_active_project() &&
                       application.active_project().metadata().scenario_kind == "bosch";
    const auto dependencies =
        bosch ? bosch_functional_dependencies() : std::vector<GuiFunctionalDependency>{};
    auto graph = build_architecture_graph(*presentation, dependencies, bosch,
                                          &application.workspace().architecture);
    const auto tasks = build_task_node_presentations(*presentation, current_workbench_theme(),
                                                     bridge_.resource_highlight());
    model_.rebuild(flat_graph(std::move(graph), application.workspace().architecture), tasks);
    for (const auto node_id : model_.allNodeIds()) {
        const auto* node_task = model_.task_presentation(node_id);
        if (node_task == nullptr) {
            continue;
        }
        if (auto* item = scene_->nodeGraphicsObject(node_id); item != nullptr) {
            item->setToolTip(node_task->assignment_valid
                                 ? QString{"Assigned to %1 with execution time %2 ticks"}
                                       .arg(node_task->resource_name)
                                       .arg(*node_task->execution_time)
                                 : QString{"%1: assignment is incomplete or inaccessible"}.arg(
                                       node_task->resource_name));
        }
    }
    synchronize_scene_selection();
    update_action_state();
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

// ---------------------------------------------------------------------------
// Context-aware Add Task
// ---------------------------------------------------------------------------

void QtArchitectureView::trigger_add_task() {
    if (context_add_position_.has_value()) {
        add_task_at(*context_add_position_);
    } else {
        const QPoint viewport_center = view_->viewport()->rect().center();
        const QPointF scene_center = view_->mapToScene(viewport_center);
        add_task_at(scene_center);
    }
}

// ---------------------------------------------------------------------------
// Hit-testing helpers
// ---------------------------------------------------------------------------

std::optional<QtNodes::NodeId> QtArchitectureView::node_at(
    const QPoint& viewport_position) const {
    auto* item = view_->itemAt(viewport_position);
    while (item != nullptr) {
        if (item->type() == QtNodes::NodeGraphicsObject::Type) {
            auto* node = static_cast<QtNodes::NodeGraphicsObject*>(item);
            return node->nodeId();
        }
        item = item->parentItem();
    }
    return std::nullopt;
}

std::optional<QtNodes::ConnectionId> QtArchitectureView::connection_at(
    const QPoint& viewport_position) const {
    auto* item = view_->itemAt(viewport_position);
    while (item != nullptr) {
        if (item->type() == QtNodes::ConnectionGraphicsObject::Type) {
            auto* conn = static_cast<QtNodes::ConnectionGraphicsObject*>(item);
            return conn->connectionId();
        }
        item = item->parentItem();
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Context menus
// ---------------------------------------------------------------------------

void QtArchitectureView::show_context_menu(const QPoint& viewport_position,
                                           const QPointF& scene_position) {
    const auto hit_node = node_at(viewport_position);
    const auto hit_connection = connection_at(viewport_position);

    // Select the hit item before showing the menu.
    if (hit_node.has_value()) {
        select_node(*hit_node);
    } else if (hit_connection.has_value()) {
        const auto conn_id = model_.connection_for(*hit_connection);
        if (conn_id.has_value()) {
            bridge_.application().structural_selection().select_connection(*conn_id);
            bridge_.notify_structural_selection_changed();
        }
    }

    context_add_position_ = scene_position;

    QMenu menu(this);

    if (!hit_node.has_value() && !hit_connection.has_value()) {
        // Empty canvas
        menu.addAction(add_task_action_);
        menu.addSeparator();
        menu.addAction(fit_action_);
        menu.addAction(actual_size_action_);
        menu.addAction(auto_layout_action_);
        menu.addAction(snap_action_);
    } else if (hit_node.has_value()) {
        // Task node
        menu.addAction(edit_action_);
        menu.addAction(duplicate_action_);
        menu.addAction(delete_action_);
        menu.addSeparator();
        menu.addAction(fit_action_);
    } else if (hit_connection.has_value()) {
        // Connection
        menu.addAction(edit_action_);
        menu.addAction(delete_action_);
        menu.addSeparator();
        menu.addAction(fit_action_);
    }

    update_action_state();
    menu.exec(view_->viewport()->mapToGlobal(viewport_position));

    // Clear temporary context position after menu closes.
    context_add_position_.reset();
    update_action_state();
}

// ---------------------------------------------------------------------------
// Duplicate, Delete, Edit
// ---------------------------------------------------------------------------

void QtArchitectureView::duplicate_selected() {
    auto& application = bridge_.application();
    const auto selected = application.structural_selection().task_id();
    if (!selected.has_value()) {
        return;
    }
    if (!edits_.duplicate_selected()) {
        return;
    }
    const auto new_task = application.structural_selection().task_id();
    if (!new_task.has_value() || *new_task == *selected) {
        return;
    }

    const auto original_node = model_.node_id_for(task_graph_node_id(*selected));
    QPointF origin{0.0, 0.0};
    if (original_node.has_value()) {
        origin = model_.nodeData(*original_node, QtNodes::NodeRole::Position).toPointF();
    }
    const auto offset = QPointF{architecture_grid_step, architecture_grid_step};
    const auto position = next_available_node_position(
        snap_architecture_position(origin + offset), QSizeF{180.0, 86.0},
        model_.occupied_rectangles(task_graph_node_id(*new_task)));
    set_task_layout_position(application.workspace().architecture, *new_task,
                             {static_cast<float>(position.x()),
                              static_cast<float>(position.y())});
    bridge_.workspace_settings_changed();
    refresh();
}

void QtArchitectureView::delete_selected_with_confirmation() {
    const auto& selection = bridge_.application().structural_selection();
    const auto task = selection.task_id();
    if (task.has_value()) {
        // Look up the task name from the draft for the confirmation dialog.
        QString task_name{QStringLiteral("the selected task")};
        if (bridge_.application().editable_system().has_value()) {
            const auto& tasks = bridge_.application().editable_system()->tasks();
            for (const auto& t : tasks) {
                if (t.id == *task) {
                    task_name = QString::fromStdString(t.name);
                    break;
                }
            }
        }
        const auto answer =
            QMessageBox::question(this, QStringLiteral("Delete Task"),
                                  QStringLiteral("Delete Task \"%1\"?\n\n"
                                                 "Related assignments and connections "
                                                 "may also be removed.")
                                      .arg(task_name),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    } else if (selection.connection().has_value()) {
        // Connection deletion not yet implemented.
        return;
    } else {
        return;
    }

    edits_.delete_selected();
    refresh();
}

void QtArchitectureView::request_edit_selected() {
    Q_EMIT editSelectionRequested();
}

// ---------------------------------------------------------------------------
// Action setup
// ---------------------------------------------------------------------------

void QtArchitectureView::build_actions() {
    fit_action_ = new QAction("Fit All", this);
    fit_action_->setObjectName("action.architecture.fit");
    fit_action_->setShortcut(QKeySequence{QStringLiteral("F")});
    fit_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(fit_action_);

    actual_size_action_ = new QAction("100%", this);
    actual_size_action_->setObjectName("action.architecture.actualSize");
    actual_size_action_->setShortcut(QKeySequence{QStringLiteral("Ctrl+0")});
    actual_size_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(actual_size_action_);

    auto_layout_action_ = new QAction("Auto Layout", this);
    auto_layout_action_->setObjectName("action.architecture.autoLayout");
    addAction(auto_layout_action_);

    snap_action_ = new QAction("Snap to Grid", this);
    snap_action_->setObjectName("action.architecture.snapToGrid");
    snap_action_->setCheckable(true);
    snap_action_->setChecked(true);
    addAction(snap_action_);

    add_task_action_ = new QAction("Add Task", this);
    add_task_action_->setObjectName("action.architecture.addTask");
    addAction(add_task_action_);

    edit_action_ = new QAction("Edit", this);
    edit_action_->setObjectName("action.architecture.edit");
    addAction(edit_action_);

    duplicate_action_ = new QAction("Duplicate", this);
    duplicate_action_->setObjectName("action.architecture.duplicate");
    duplicate_action_->setShortcut(QKeySequence{QStringLiteral("Ctrl+D")});
    duplicate_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(duplicate_action_);

    delete_action_ = new QAction("Delete", this);
    delete_action_->setObjectName("action.architecture.delete");
    delete_action_->setShortcut(QKeySequence::Delete);
    delete_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(delete_action_);
}

// ---------------------------------------------------------------------------
// Action-state consolidated update
// ---------------------------------------------------------------------------

void QtArchitectureView::update_action_state() {
    const bool can_edit = edits_.editing_enabled() &&
                          edits_.edit_policy() == ProjectSystemEditPolicy::Generic;

    add_task_action_->setEnabled(can_edit);

    const auto task_selected =
        bridge_.application().structural_selection().task_id().has_value();
    const auto connection_selected =
        bridge_.application().structural_selection().connection().has_value();
    const bool has_selection = task_selected || connection_selected;

    edit_action_->setEnabled(has_selection);
    duplicate_action_->setEnabled(can_edit && task_selected);
    // Connection deletion not yet implemented; disable for connections.
    delete_action_->setEnabled(can_edit && task_selected);
}

} // namespace cpssim::qt
