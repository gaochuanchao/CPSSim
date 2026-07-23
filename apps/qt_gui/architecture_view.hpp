/*** Native QtNodes flat Architecture view bound to WorkbenchApplication. ***/
#pragma once

#ifndef Q_MOC_RUN
#include "apps/qt_gui/architecture_model.hpp"
#include <functional>
#include <memory>
#include <optional>
#endif

#include <QComboBox>
#include <QPointF>
#include <QWidget>

class QAction;

// QtNodes::NodeId and ConnectionId are typedefs (unsigned int / struct),
// included transitively through architecture_model.hpp.
#ifndef Q_MOC_RUN
namespace QtNodes {
class BasicGraphicsScene;
class GraphicsView;
} // namespace QtNodes
#endif

namespace cpssim::qt {

class QtWorkbenchBridge;
class QtStructuralEditController;

class QtArchitectureView final : public QWidget {
    Q_OBJECT

  public:
    explicit QtArchitectureView(QtWorkbenchBridge& bridge,
                                QtStructuralEditController& edits,
                                QWidget* parent = nullptr);
    ~QtArchitectureView() override;

    QtArchitectureGraphModel& graph_model() noexcept { return model_; }
    const QtArchitectureGraphModel& graph_model() const noexcept { return model_; }
    QtNodes::BasicGraphicsScene& graphics_scene() noexcept { return *scene_; }
    void refresh();
    std::optional<TaskId> add_task_at(QPointF scene_position);
    void place_task_near_view_center(TaskId task_id);
    GuiConnectionKind current_link_type() const;

  Q_SIGNALS:
    void editSelectionRequested();

  private:
    using ContextMenuHandler =
        std::function<void(const QPoint& viewport_position, const QPointF& scene_position)>;

    void select_node(QtNodes::NodeId node_id);
    void select_scene_item();
    void persist_node_position(GuiGraphNodeId entity, QPointF position);
    void auto_layout();
    void snap_node_position(QtNodes::NodeId node_id, QPointF position);
    void commit_graphics_node_positions();
    void synchronize_scene_selection();
    void trigger_add_task();
    void refresh_appearance();
    void update_action_state();
    void build_actions();

    void show_context_menu(const QPoint& viewport_position,
                           const QPointF& scene_position);

    std::optional<QtNodes::NodeId> node_at(const QPoint& viewport_position) const;
    std::optional<QtNodes::ConnectionId> connection_at(
        const QPoint& viewport_position) const;

    void duplicate_selected();
    void delete_selected_with_confirmation();
    void request_edit_selected();

    QtWorkbenchBridge& bridge_;
    QtStructuralEditController& edits_;
    QtArchitectureGraphModel model_;
    std::unique_ptr<QtNodes::BasicGraphicsScene> scene_;
    std::unique_ptr<QtNodes::GraphicsView> view_;
    bool snap_to_grid_{true};
    bool synchronizing_scene_selection_{false};
    bool rebuilding_scene_{false};
    bool committing_node_positions_{false};

    QAction* add_task_action_{nullptr};
    QAction* edit_action_{nullptr};
    QAction* duplicate_action_{nullptr};
    QAction* delete_action_{nullptr};
    QAction* fit_action_{nullptr};
    QAction* actual_size_action_{nullptr};
    QAction* auto_layout_action_{nullptr};
    QAction* snap_action_{nullptr};
    QComboBox* link_type_selector_{nullptr};

    std::optional<QPointF> context_add_position_;
};

} // namespace cpssim::qt
