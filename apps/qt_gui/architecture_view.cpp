/*** Render CPSSim tasks and semantic connections through flat QtNodes. ***/
#include "apps/qt_gui/architecture_view.hpp"

#include "apps/qt_gui/architecture_connection_painter.hpp"
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
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
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
    using ContextMenuHandler =
        std::function<void(const QPoint& viewport_position, const QPointF& scene_position)>;
    using NodeMoveFinishedHandler = std::function<void()>;

    explicit QtArchitectureGraphicsView(QtNodes::BasicGraphicsScene* scene,
                                        QWidget* parent = nullptr)
        : QtNodes::GraphicsView(scene, parent) {
        disable_qtnodes_conflicting_shortcuts();
    }

    void set_context_menu_handler(ContextMenuHandler handler) {
        context_menu_handler_ = std::move(handler);
    }

    void set_node_move_finished_handler(NodeMoveFinishedHandler handler) {
        node_move_finished_handler_ = std::move(handler);
    }

  protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override {
        painter->save();

        painter->fillRect(rect, palette().color(QPalette::Base));

        painter->setRenderHint(QPainter::Antialiasing, false);

        auto minor_color = palette().color(QPalette::Mid);
        auto major_color = minor_color;

        minor_color.setAlpha(35);
        major_color.setAlpha(75);

        QPen minor_pen{minor_color};
        minor_pen.setWidthF(0.0);

        QPen major_pen{major_color};
        major_pen.setWidthF(0.0);

        const auto first_x =
            std::floor(rect.left() / architecture_grid_step) * architecture_grid_step;
        const auto first_y =
            std::floor(rect.top() / architecture_grid_step) * architecture_grid_step;

        for (qreal x = first_x; x <= rect.right(); x += architecture_grid_step) {
            const auto index =
                static_cast<qint64>(std::llround(x / architecture_grid_step));
            painter->setPen(index % architecture_major_grid_every == 0 ? major_pen : minor_pen);
            painter->drawLine(QLineF{x, rect.top(), x, rect.bottom()});
        }

        for (qreal y = first_y; y <= rect.bottom(); y += architecture_grid_step) {
            const auto index =
                static_cast<qint64>(std::llround(y / architecture_grid_step));
            painter->setPen(index % architecture_major_grid_every == 0 ? major_pen : minor_pen);
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

    void mouseReleaseEvent(QMouseEvent* event) override {
        QtNodes::GraphicsView::mouseReleaseEvent(event);
        if (node_move_finished_handler_) {
            node_move_finished_handler_();
        }
    }

  private:
    void disable_qtnodes_conflicting_shortcuts() {
        // Build the set of shortcut values that QtNodes may install but CPSSim
        // must own exclusively for structural editing.
        const auto undo_bindings = QKeySequence::keyBindings(QKeySequence::Undo);
        const auto redo_bindings = QKeySequence::keyBindings(QKeySequence::Redo);
        const auto cut_bindings = QKeySequence::keyBindings(QKeySequence::Cut);
        const auto copy_bindings = QKeySequence::keyBindings(QKeySequence::Copy);
        const auto paste_bindings = QKeySequence::keyBindings(QKeySequence::Paste);
        const QKeySequence delete_binding{QKeySequence::Delete};
        const QKeySequence duplicate_binding{Qt::CTRL | Qt::Key_D};
        const QKeySequence escape_binding{Qt::Key_Escape};

        for (auto* action : actions()) {
            if (action == nullptr) {
                continue;
            }
            const auto shortcuts = action->shortcuts();
            bool is_conflicting = false;
            for (const auto& shortcut : shortcuts) {
                if (undo_bindings.contains(shortcut) || redo_bindings.contains(shortcut) ||
                    cut_bindings.contains(shortcut) || copy_bindings.contains(shortcut) ||
                    paste_bindings.contains(shortcut) || shortcut == delete_binding ||
                    shortcut == duplicate_binding || shortcut == escape_binding) {
                    is_conflicting = true;
                    break;
                }
            }
            if (is_conflicting) {
                action->setShortcuts({});
            }
        }
    }

    ContextMenuHandler context_menu_handler_;
    NodeMoveFinishedHandler node_move_finished_handler_;
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
    scene_->setConnectionPainter(
        std::make_unique<QtArchitectureConnectionPainter>(model_));

    auto* toolbar = new QToolBar("Architecture", this);
    toolbar->setObjectName("toolbar.architecture");
    build_actions();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(toolbar);
    toolbar->addAction(fit_action_);
    toolbar->addAction(center_view_action_);
    toolbar->addAction(actual_size_action_);
    toolbar->addAction(auto_layout_action_);
    toolbar->addAction(snap_action_);
    toolbar->addSeparator();
    toolbar->addAction(add_task_action_);
    toolbar->addSeparator();
    link_type_selector_ = new QComboBox(toolbar);
    link_type_selector_->setObjectName("toolbar.architecture.linkType");
    link_type_selector_->addItem("Communication");
    link_type_selector_->addItem("Logical");
    link_type_selector_->setToolTip("Link type for newly created connections");
    toolbar->addWidget(new QLabel(" New link type:", toolbar));
    toolbar->addWidget(link_type_selector_);
    layout->addWidget(view_.get());

    auto* graphics_view = static_cast<QtArchitectureGraphicsView*>(view_.get());
    graphics_view->set_context_menu_handler(
        [this](const QPoint& vp, const QPointF& sp) { show_context_menu(vp, sp); });

    connect(fit_action_, &QAction::triggered, view_.get(), &QtNodes::GraphicsView::zoomFitAll);
    connect(center_view_action_, &QAction::triggered, this, &QtArchitectureView::center_graph_in_view);
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
    graphics_view->set_node_move_finished_handler(
        [this] { commit_graphics_node_positions(); });
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { refresh(); });
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this, [this] {
        synchronize_scene_selection();
        update_action_state();
    });
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::resourceHighlightChanged, this,
            &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::workspaceChanged, this, &QtArchitectureView::refresh);
    connect(&bridge_, &QtWorkbenchBridge::appearanceChanged, this,
            &QtArchitectureView::refresh_appearance);
    model_.set_position_changed([this](GuiGraphNodeId entity, QPointF position) {
        persist_node_position(entity, position);
    });
    model_.set_structural_edit_enabled([this] {
        return edits_.editing_enabled() &&
               edits_.edit_policy() == ProjectSystemEditPolicy::Generic;
    });
    model_.set_connection_create_requested([this](TaskId source, TaskId destination) {
        const int kind_int = current_link_type() == GuiConnectionKind::Logical ? 1 : 0;
        return edits_.create_connection(source, destination, kind_int);
    });
    model_.set_connection_delete_requested([this](const GuiConnectionId& connection) {
        return edits_.delete_connection(connection);
    });
    update_action_state();
    refresh();
}

void QtArchitectureView::select_scene_item() {
    if (synchronizing_scene_selection_) {
        return;
    }
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
        {
            QScopedValueRollback<bool> rb{rebuilding_scene_, true};
            const QSignalBlocker scene_signals{scene_.get()};
            model_.rebuild({});
        }
        update_action_state();
        return;
    }

    const auto bosch = application.has_active_project() &&
                       application.active_project().metadata().scenario_kind == "bosch";
    const auto dependencies =
        bosch ? bosch_functional_dependencies() : std::vector<GuiFunctionalDependency>{};
    auto graph = build_architecture_graph(*presentation, dependencies, bosch,
                                          &application.workspace().architecture);
    const auto tasks = build_task_node_presentations(*presentation, current_workbench_theme(),
                                                     bridge_.resource_highlight());
    {
        QScopedValueRollback<bool> rb{rebuilding_scene_, true};
        const QSignalBlocker scene_signals{scene_.get()};
        model_.rebuild(flat_graph(std::move(graph), application.workspace().architecture), tasks);
    }
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
    // Set pointing-hand cursor on all connection graphics objects.
    {
        std::unordered_set<QtNodes::ConnectionId> seen;
        for (const auto nid : model_.allNodeIds()) {
            for (const auto& cid : model_.allConnectionIds(nid)) {
                if (seen.insert(cid).second) {
                    if (auto* cgo = scene_->connectionGraphicsObject(cid); cgo != nullptr) {
                        cgo->setCursor(Qt::PointingHandCursor);
                    }
                }
            }
        }
    }
    synchronize_scene_selection();
    update_action_state();

    // Detect project change for initial automatic centering
    const auto active_root = application.has_active_project()
                                 ? std::optional{application.active_project().root()}
                                 : std::nullopt;
    if (active_root != observed_project_root_) {
        observed_project_root_ = active_root;
        if (active_root.has_value()) {
            schedule_initial_center();
        } else {
            initial_center_pending_ = false;
        }
    }
}

void QtArchitectureView::select_node(QtNodes::NodeId node_id) {
    if (synchronizing_scene_selection_) {
        return;
    }
    const auto entity = model_.entity_for(node_id);
    if (!entity.has_value() || entity->kind != GuiGraphNodeKind::Task) {
        return;
    }
    bridge_.application().structural_selection().select_task(TaskId{entity->entity_value});
    bridge_.notify_structural_selection_changed();
}

void QtArchitectureView::persist_node_position(GuiGraphNodeId entity, QPointF position) {
    if (rebuilding_scene_) {
        return;
    }
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
    if (rebuilding_scene_) {
        return;
    }
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

void QtArchitectureView::commit_graphics_node_positions() {
    // Called after mouse release to persist the actual graphics-object
    // positions to the model and workspace.  The QtNodes library moves
    // NodeGraphicsObject via QGraphicsObject::mouseMoveEvent (ItemIsMovable)
    // without calling setNodeData(), so the model position can become stale.
    if (rebuilding_scene_ || committing_node_positions_) {
        return;
    }
    committing_node_positions_ = true;

    bool any_changed = false;
    for (const auto node_id : model_.allNodeIds()) {
        auto* item = scene_->nodeGraphicsObject(node_id);
        if (item == nullptr) {
            continue;
        }
        const QPointF graphics_pos = item->pos();
        const QPointF model_pos =
            model_.nodeData(node_id, QtNodes::NodeRole::Position).toPointF();

        QPointF final_pos = graphics_pos;
        if (snap_to_grid_) {
            final_pos = snap_architecture_position(graphics_pos);
        }

        if (final_pos != model_pos) {
            // setNodeData triggers position_changed_ -> persist_node_position,
            // which writes to GuiArchitectureWorkspace.
            static_cast<void>(
                model_.setNodeData(node_id, QtNodes::NodeRole::Position, final_pos));
            any_changed = true;
        }
    }

    committing_node_positions_ = false;

    if (any_changed) {
        bridge_.workspace_settings_changed();
    }
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

GuiConnectionKind QtArchitectureView::current_link_type() const {
    return link_type_selector_->currentIndex() == 1 ? GuiConnectionKind::Logical
                                                    : GuiConnectionKind::Communication;
}

void QtArchitectureView::synchronize_scene_selection() {
    synchronizing_scene_selection_ = true;
    scene_->clearSelection();

    const auto& selection = bridge_.application().structural_selection();

    // --- Connection selections ---
    if (selection.kind() == StructuralSelectionKind::Connection) {
        const auto conn = selection.connection();
        if (conn.has_value()) {
            const auto qt_id = model_.connection_id_for(*conn);
            if (qt_id.has_value()) {
                if (auto* cgo = scene_->connectionGraphicsObject(*qt_id); cgo != nullptr) {
                    cgo->setSelected(true);
                }
            }
        }
        synchronizing_scene_selection_ = false;
        return;
    }

    // --- MessageRoute selections (find matching communication/link connection) ---
    if (selection.kind() == StructuralSelectionKind::MessageRoute) {
        const auto mr = selection.message_route();
        if (mr.has_value()) {
            const GuiConnectionId semantic{GuiConnectionKind::Communication,
                                           mr->source_task_id, mr->destination_task_id};
            const auto qt_id = model_.connection_id_for(semantic);
            if (qt_id.has_value()) {
                if (auto* cgo = scene_->connectionGraphicsObject(*qt_id); cgo != nullptr) {
                    cgo->setSelected(true);
                }
            }
        }
        synchronizing_scene_selection_ = false;
        return;
    }

    // --- Task selections ---
    const auto task = selection.task_id();
    if (task.has_value() && selection.kind() == StructuralSelectionKind::Task) {
        const auto node_id = model_.node_id_for(task_graph_node_id(*task));
        if (node_id.has_value()) {
            if (auto* item = scene_->nodeGraphicsObject(*node_id); item != nullptr) {
                item->setSelected(true);
            }
        }
    }
    // Other selection kinds (System, Resource, Section) leave canvas cleared.

    synchronizing_scene_selection_ = false;
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

void QtArchitectureView::refresh_appearance() {
    refresh();
    view_->resetCachedContent();
    view_->viewport()->update();
}

// ---------------------------------------------------------------------------
// Graph centering
// ---------------------------------------------------------------------------

std::optional<QRectF> QtArchitectureView::graph_node_bounds() const {
    std::optional<QRectF> bounds;
    for (const auto node_id : model_.allNodeIds()) {
        auto* item = scene_->nodeGraphicsObject(node_id);
        if (item == nullptr) {
            continue;
        }
        const auto rect = item->sceneBoundingRect();
        if (bounds.has_value()) {
            *bounds = bounds->united(rect);
        } else {
            bounds = rect;
        }
    }
    return bounds;
}

void QtArchitectureView::center_graph_in_view() {
    const auto bounds = graph_node_bounds();
    if (!bounds.has_value()) {
        return;
    }
    view_->centerOn(bounds->center());
}

void QtArchitectureView::schedule_initial_center() {
    initial_center_pending_ = true;
    // Use a single-shot timer to center after the viewport geometry and
    // graphics objects are ready.
    QTimer::singleShot(0, this, [this] {
        if (!initial_center_pending_) {
            return;
        }
        initial_center_pending_ = false;
        // If the view is already visible, center immediately.
        // If hidden, the showEvent will apply centering and clear the flag.
        if (isVisible()) {
            center_graph_in_view();
        }
        // When hidden: leave flag false — showEvent handles it
    });
}

void QtArchitectureView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Center only if this is the very first show after a project change.
    // The flag is set the first time the project is loaded and cleared
    // after the first successful centering.
    QTimer::singleShot(0, this, [this] {
        if (!initial_center_pending_) {
            return;
        }
        initial_center_pending_ = false;
        center_graph_in_view();
    });
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
        menu.addAction(center_view_action_);
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
        menu.addAction(center_view_action_);
    } else if (hit_connection.has_value()) {
        // Connection
        menu.addAction(edit_action_);
        menu.addAction(delete_action_);
        menu.addSeparator();
        menu.addAction(fit_action_);
        menu.addAction(center_view_action_);
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
        edits_.delete_selected();
        refresh();
        return;
    }

    const auto conn = selection.connection();
    if (conn.has_value() &&
        (conn->kind == GuiConnectionKind::Communication || conn->kind == GuiConnectionKind::Logical)) {
        const auto link_type = conn->kind == GuiConnectionKind::Communication ? "communication" : "logical";
        const auto answer =
            QMessageBox::question(this, QStringLiteral("Delete Connection"),
                                  QStringLiteral("Delete this %1 link?").arg(link_type),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        edits_.delete_connection(*conn);
        bridge_.notify_structural_selection_changed();
        refresh();
        return;
    }

    // Non-domain or invalid selection — nothing to delete.
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

    center_view_action_ = new QAction("Center View", this);
    center_view_action_->setObjectName("action.architecture.centerView");
    center_view_action_->setToolTip(QStringLiteral("Center the graph without changing zoom"));
    addAction(center_view_action_);

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

    const bool has_nodes = !model_.allNodeIds().empty();
    center_view_action_->setEnabled(has_nodes);

    const auto& selection = bridge_.application().structural_selection();
    const auto task_selected = selection.task_id().has_value();
    const auto conn = selection.connection();
    const bool connection_selected =
        conn.has_value() &&
        (conn->kind == GuiConnectionKind::Communication || conn->kind == GuiConnectionKind::Logical);
    const bool has_selection = task_selected || connection_selected;

    edit_action_->setEnabled(has_selection);
    duplicate_action_->setEnabled(can_edit && task_selected);
    delete_action_->setEnabled(can_edit && (task_selected || connection_selected));
}

} // namespace cpssim::qt
