/*** Flat QtNodes adapter for CPSSim's immutable Architecture presentation. ***/
#pragma once

#include "cpssim/gui/architecture_graph.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <QtNodes/AbstractGraphModel>
#include <QtNodes/NodeData>

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cpssim::qt {

struct QtTaskNodePresentation {
    TaskId task_id;
    std::optional<ResourceId> resource_id;
    QString resource_name;
    QColor accent;
    Tick period{};
    Tick deadline{};
    std::optional<Tick> execution_time;
    bool assignment_valid{false};
    bool highlighted{false};
};

std::vector<QtTaskNodePresentation>
build_task_node_presentations(const ExperimentPresentationSnapshot& experiment, GuiTheme theme,
                              std::optional<ResourceId> highlighted_resource = std::nullopt);

class QtNodeIdMap {
  public:
    QtNodes::NodeId adapter_id(GuiGraphNodeId entity);
    std::optional<QtNodes::NodeId> find_adapter_id(GuiGraphNodeId entity) const;
    std::optional<GuiGraphNodeId> entity_id(QtNodes::NodeId adapter_id) const;
    QtNodes::NodeId next_adapter_id();

  private:
    std::map<GuiGraphNodeId, QtNodes::NodeId> entity_to_adapter_;
    std::map<QtNodes::NodeId, GuiGraphNodeId> adapter_to_entity_;
    QtNodes::NodeId next_{1};
};

QPointF next_available_node_position(QPointF requested, const QSizeF& size,
                                     const std::vector<QRectF>& occupied, qreal offset = 24.0);

class QtArchitectureGraphModel final : public QtNodes::AbstractGraphModel {
  public:
    using PositionChanged = std::function<void(GuiGraphNodeId, QPointF)>;

    explicit QtArchitectureGraphModel(QObject* parent = nullptr);

    void rebuild(const GuiArchitectureGraph& graph,
                 const std::vector<QtTaskNodePresentation>& task_presentations = {});
    void set_position_changed(PositionChanged callback) { position_changed_ = std::move(callback); }
    std::optional<QtNodes::NodeId> node_id_for(GuiGraphNodeId entity) const;
    std::optional<GuiGraphNodeId> entity_for(QtNodes::NodeId node_id) const;
    std::optional<GuiConnectionId> connection_for(const QtNodes::ConnectionId& connection_id) const;
    std::size_t node_count() const noexcept { return nodes_.size(); }
    std::size_t connection_count() const noexcept { return connections_.size(); }
    std::vector<QRectF> occupied_rectangles() const;
    const QtTaskNodePresentation* task_presentation(QtNodes::NodeId node_id) const;

    QtNodes::NodeId newNodeId() override;
    std::unordered_set<QtNodes::NodeId> allNodeIds() const override;
    std::unordered_set<QtNodes::ConnectionId>
    allConnectionIds(QtNodes::NodeId node_id) const override;
    std::unordered_set<QtNodes::ConnectionId> connections(QtNodes::NodeId node_id,
                                                          QtNodes::PortType port_type,
                                                          QtNodes::PortIndex index) const override;
    bool connectionExists(QtNodes::ConnectionId connection_id) const override;
    QtNodes::NodeId addNode(const QString node_type = {}) override;
    bool connectionPossible(QtNodes::ConnectionId connection_id) const override;
    void addConnection(QtNodes::ConnectionId connection_id) override;
    bool nodeExists(QtNodes::NodeId node_id) const override;
    QVariant nodeData(QtNodes::NodeId node_id, QtNodes::NodeRole role) const override;
    bool setNodeData(QtNodes::NodeId node_id, QtNodes::NodeRole role, QVariant value) override;
    QVariant portData(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                      QtNodes::PortIndex index, QtNodes::PortRole role) const override;
    bool setPortData(QtNodes::NodeId node_id, QtNodes::PortType port_type, QtNodes::PortIndex index,
                     const QVariant& value, QtNodes::PortRole role) override;
    bool deleteConnection(QtNodes::ConnectionId connection_id) override;
    bool deleteNode(QtNodes::NodeId node_id) override;
    bool loopsEnabled() const override { return true; }

  private:
    struct NodeRecord {
        GuiGraphNodeId entity;
        QString caption;
        QPointF position;
        QSize size;
        QtNodes::PortCount input_count{};
        QtNodes::PortCount output_count{};
        std::optional<QtTaskNodePresentation> presentation;
    };

    QtNodeIdMap ids_;
    std::map<QtNodes::NodeId, NodeRecord> nodes_;
    std::vector<QtNodes::ConnectionId> connections_;
    std::vector<std::pair<QtNodes::ConnectionId, GuiConnectionId>> connection_ids_;
    PositionChanged position_changed_;
};

} // namespace cpssim::qt
