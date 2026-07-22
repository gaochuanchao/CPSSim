/*** Native QtNodes flat Architecture view bound to WorkbenchApplication. ***/
#pragma once

#include "apps/qt_gui/architecture_model.hpp"

#include <QPointF>
#include <QWidget>

#include <memory>

namespace QtNodes {
class BasicGraphicsScene;
class GraphicsView;
} // namespace QtNodes

namespace cpssim::qt {

class QtWorkbenchBridge;

class QtArchitectureView final : public QWidget {
  public:
    explicit QtArchitectureView(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    ~QtArchitectureView() override;

    QtArchitectureGraphModel& graph_model() noexcept { return model_; }
    const QtArchitectureGraphModel& graph_model() const noexcept { return model_; }
    QtNodes::BasicGraphicsScene& graphics_scene() noexcept { return *scene_; }
    void refresh();
    std::optional<TaskId> add_task_at(QPointF scene_position);

  private:
    void select_node(QtNodes::NodeId node_id);
    void select_scene_item();
    void persist_node_position(GuiGraphNodeId entity, QPointF position);
    void add_task_at_view_center();
    void synchronize_scene_selection();

    QtWorkbenchBridge& bridge_;
    QtArchitectureGraphModel model_;
    std::unique_ptr<QtNodes::BasicGraphicsScene> scene_;
    std::unique_ptr<QtNodes::GraphicsView> view_;
};

} // namespace cpssim::qt
