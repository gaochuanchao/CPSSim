/*** Implement the read-only semantic QtNodes Architecture adapter. ***/
#include "apps/qt_gui/architecture_model.hpp"

#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cpssim::qt {

QtNodes::NodeId QtNodeIdMap::adapter_id(GuiGraphNodeId entity) {
    if (const auto found = entity_to_adapter_.find(entity); found != entity_to_adapter_.end()) {
        return found->second;
    }
    const auto id = next_adapter_id();
    entity_to_adapter_.emplace(entity, id);
    adapter_to_entity_.emplace(id, entity);
    return id;
}

std::optional<QtNodes::NodeId> QtNodeIdMap::find_adapter_id(GuiGraphNodeId entity) const {
    const auto found = entity_to_adapter_.find(entity);
    return found == entity_to_adapter_.end() ? std::nullopt
                                             : std::optional<QtNodes::NodeId>{found->second};
}

std::optional<GuiGraphNodeId> QtNodeIdMap::entity_id(QtNodes::NodeId adapter_id) const {
    const auto found = adapter_to_entity_.find(adapter_id);
    return found == adapter_to_entity_.end() ? std::nullopt
                                             : std::optional<GuiGraphNodeId>{found->second};
}

QtNodes::NodeId QtNodeIdMap::next_adapter_id() {
    while (adapter_to_entity_.contains(next_)) {
        if (next_ == QtNodes::InvalidNodeId - 1) {
            throw std::overflow_error{"QtNodes ID space is exhausted"};
        }
        ++next_;
    }
    return next_++;
}

QPointF next_available_node_position(QPointF requested, const QSizeF& size,
                                     const std::vector<QRectF>& occupied, qreal offset) {
    auto candidate = requested;
    constexpr int maximum_attempts = 4'096;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        const QRectF rectangle{candidate, size};
        if (std::none_of(occupied.begin(), occupied.end(),
                         [&](const auto& existing) { return existing.intersects(rectangle); })) {
            return candidate;
        }
        candidate += QPointF{offset, offset};
    }
    return candidate;
}

QtArchitectureGraphModel::QtArchitectureGraphModel(QObject* parent)
    : QtNodes::AbstractGraphModel() {
    setParent(parent);
}

void QtArchitectureGraphModel::rebuild(const GuiArchitectureGraph& graph) {
    nodes_.clear();
    connections_.clear();
    std::size_t flat_index = 0;
    for (const auto& node : graph.nodes) {
        if (node.kind != GuiGraphNodeKind::Task) {
            continue;
        }
        const auto id = ids_.adapter_id(node.id);
        auto position = QPointF{node.position.x, node.position.y};
        if (position.isNull()) {
            position = QPointF{40.0 + static_cast<qreal>(flat_index % 4) * 240.0,
                               40.0 + static_cast<qreal>(flat_index / 4) * 140.0};
        }
        nodes_.emplace(id, NodeRecord{.entity = node.id,
                                      .caption = QString::fromStdString(node.label),
                                      .position = position,
                                      .size = QSize{180, 86}});
        ++flat_index;
    }

    std::map<QtNodes::NodeId, QtNodes::PortIndex> next_input;
    std::map<QtNodes::NodeId, QtNodes::PortIndex> next_output;
    for (const auto& edge : graph.edges) {
        if (edge.kind == GuiGraphEdgeKind::Assignment) {
            continue;
        }
        const auto source = ids_.find_adapter_id(edge.source);
        const auto destination = ids_.find_adapter_id(edge.destination);
        if (!source.has_value() || !destination.has_value() || !nodes_.contains(*source) ||
            !nodes_.contains(*destination)) {
            continue;
        }
        const auto output_port = next_output[*source]++;
        const auto input_port = next_input[*destination]++;
        connections_.push_back({*source, output_port, *destination, input_port});
        nodes_.at(*source).output_count = next_output[*source];
        nodes_.at(*destination).input_count = next_input[*destination];
    }
    Q_EMIT modelReset();
}

std::optional<QtNodes::NodeId> QtArchitectureGraphModel::node_id_for(GuiGraphNodeId entity) const {
    const auto id = ids_.find_adapter_id(entity);
    return id.has_value() && nodes_.contains(*id) ? id : std::nullopt;
}

std::optional<GuiGraphNodeId> QtArchitectureGraphModel::entity_for(QtNodes::NodeId node_id) const {
    return nodes_.contains(node_id) ? ids_.entity_id(node_id) : std::nullopt;
}

std::vector<QRectF> QtArchitectureGraphModel::occupied_rectangles() const {
    std::vector<QRectF> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        static_cast<void>(id);
        result.emplace_back(node.position, node.size);
    }
    return result;
}

QtNodes::NodeId QtArchitectureGraphModel::newNodeId() { return ids_.next_adapter_id(); }

std::unordered_set<QtNodes::NodeId> QtArchitectureGraphModel::allNodeIds() const {
    std::unordered_set<QtNodes::NodeId> result;
    for (const auto& [id, node] : nodes_) {
        static_cast<void>(node);
        result.insert(id);
    }
    return result;
}

std::unordered_set<QtNodes::ConnectionId>
QtArchitectureGraphModel::allConnectionIds(QtNodes::NodeId node_id) const {
    std::unordered_set<QtNodes::ConnectionId> result;
    for (const auto& connection : connections_) {
        if (connection.outNodeId == node_id || connection.inNodeId == node_id) {
            result.insert(connection);
        }
    }
    return result;
}

std::unordered_set<QtNodes::ConnectionId>
QtArchitectureGraphModel::connections(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                                      QtNodes::PortIndex index) const {
    std::unordered_set<QtNodes::ConnectionId> result;
    for (const auto& connection : connections_) {
        if ((port_type == QtNodes::PortType::Out && connection.outNodeId == node_id &&
             connection.outPortIndex == index) ||
            (port_type == QtNodes::PortType::In && connection.inNodeId == node_id &&
             connection.inPortIndex == index)) {
            result.insert(connection);
        }
    }
    return result;
}

bool QtArchitectureGraphModel::connectionExists(QtNodes::ConnectionId connection_id) const {
    return std::find(connections_.begin(), connections_.end(), connection_id) != connections_.end();
}

QtNodes::NodeId QtArchitectureGraphModel::addNode(const QString) { return QtNodes::InvalidNodeId; }

bool QtArchitectureGraphModel::connectionPossible(QtNodes::ConnectionId) const { return false; }

void QtArchitectureGraphModel::addConnection(QtNodes::ConnectionId) {}

bool QtArchitectureGraphModel::nodeExists(QtNodes::NodeId node_id) const {
    return nodes_.contains(node_id);
}

QVariant QtArchitectureGraphModel::nodeData(QtNodes::NodeId node_id, QtNodes::NodeRole role) const {
    const auto found = nodes_.find(node_id);
    if (found == nodes_.end()) {
        return {};
    }
    const auto& node = found->second;
    switch (role) {
    case QtNodes::NodeRole::Type:
        return QStringLiteral("Task");
    case QtNodes::NodeRole::Position:
        return node.position;
    case QtNodes::NodeRole::Size:
        return node.size;
    case QtNodes::NodeRole::CaptionVisible:
        return true;
    case QtNodes::NodeRole::Caption:
        return node.caption;
    case QtNodes::NodeRole::InPortCount:
        return node.input_count;
    case QtNodes::NodeRole::OutPortCount:
        return node.output_count;
    case QtNodes::NodeRole::Widget:
        return QVariant::fromValue(static_cast<QWidget*>(nullptr));
    case QtNodes::NodeRole::LabelVisible:
        return false;
    case QtNodes::NodeRole::Label:
        return QString{};
    default:
        return {};
    }
}

bool QtArchitectureGraphModel::setNodeData(QtNodes::NodeId node_id, QtNodes::NodeRole role,
                                           QVariant value) {
    const auto found = nodes_.find(node_id);
    if (found == nodes_.end() || role != QtNodes::NodeRole::Position) {
        return false;
    }
    found->second.position = value.toPointF();
    if (position_changed_) {
        position_changed_(found->second.entity, found->second.position);
    }
    Q_EMIT nodePositionUpdated(node_id);
    return true;
}

QVariant QtArchitectureGraphModel::portData(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                                            QtNodes::PortIndex index,
                                            QtNodes::PortRole role) const {
    if (!nodes_.contains(node_id)) {
        return {};
    }
    const auto count = port_type == QtNodes::PortType::In ? nodes_.at(node_id).input_count
                                                          : nodes_.at(node_id).output_count;
    if (index >= count) {
        return {};
    }
    switch (role) {
    case QtNodes::PortRole::DataType:
        return QVariant::fromValue(QtNodes::NodeDataType{QStringLiteral("cpssim.connection"),
                                                         QStringLiteral("CPSSim connection")});
    case QtNodes::PortRole::ConnectionPolicyRole:
        return QVariant::fromValue(QtNodes::ConnectionPolicy::Many);
    case QtNodes::PortRole::CaptionVisible:
        return false;
    case QtNodes::PortRole::Caption:
        return QString{};
    default:
        return {};
    }
}

bool QtArchitectureGraphModel::setPortData(QtNodes::NodeId, QtNodes::PortType, QtNodes::PortIndex,
                                           const QVariant&, QtNodes::PortRole) {
    return false;
}

bool QtArchitectureGraphModel::deleteConnection(QtNodes::ConnectionId) { return false; }

bool QtArchitectureGraphModel::deleteNode(QtNodes::NodeId) { return false; }

} // namespace cpssim::qt
