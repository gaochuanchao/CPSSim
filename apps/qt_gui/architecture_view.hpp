/*** Native QtNodes flat Architecture view bound to WorkbenchApplication. ***/
#pragma once

#include "apps/qt_gui/architecture_model.hpp"

#include <QPointF>
#include <QWidget>

#include <memory>

class QAction;

namespace QtNodes {
class BasicGraphicsScene;
class GraphicsView;
} // namespace QtNodes

namespace cpssim::qt {

class QtWorkbenchBridge;
class QtStructuralEditController;

class QtArchitectureView final : public QWidget {
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

  private:
    void select_node(QtNodes::NodeId node_id);
    void select_scene_item();
    void persist_node_position(GuiGraphNodeId entity, QPointF position);
    void auto_layout();
    void snap_node_position(QtNodes::NodeId node_id, QPointF position);
    void synchronize_scene_selection();
    void add_task_near_view_center();
    void update_add_task_action_state();

    QtWorkbenchBridge& bridge_;
    QtStructuralEditController& edits_;
    QtArchitectureGraphModel model_;
    std::unique_ptr<QtNodes::BasicGraphicsScene> scene_;
    std::unique_ptr<QtNodes::GraphicsView> view_;
    bool snap_to_grid_{true};
    QAction* add_task_action_{nullptr};
};

} // namespace cpssim::qt
