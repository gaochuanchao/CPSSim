/*** Implement the read-only semantic QtNodes Architecture adapter. ***/
#include "apps/qt_gui/architecture_model.hpp"

#include "apps/qt_gui/workbench_style.hpp"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/StyleCollection>

#include <QApplication>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cpssim::qt {
namespace {

// Port caption helpers.  Returns "in" / "out" when count == 1,
// or "in[N]" / "out[N]" when count > 1.
QString port_caption(QtNodes::PortType port_type, QtNodes::PortCount count,
                     QtNodes::PortIndex index) {
    if (index >= count) {
        return {};
    }
    const auto prefix = port_type == QtNodes::PortType::In ? QStringLiteral("in")
                                                           : QStringLiteral("out");
    if (count == 1) {
        return prefix;
    }
    return QStringLiteral("%1[%2]").arg(prefix).arg(index);
}

QPalette active_palette_for_theme(GuiTheme theme) {
    // Use the stored palette rather than qApp->palette() to avoid
    // depending on when the theme was last applied.
    return workbench_palette(theme);
}

QVariantMap build_theme_aware_node_style(GuiTheme theme) {
    auto style = QtNodes::StyleCollection::nodeStyle();
    const auto palette = active_palette_for_theme(theme);

    // Background / gradient: use Base as the node surface colour.
    const auto base = palette.color(QPalette::Base);
    style.setBackgroundColor(base);

    // Boundary / border
    style.NormalBoundaryColor = palette.color(QPalette::Mid);
    style.SelectedBoundaryColor = palette.color(QPalette::Highlight);

    // Text
    style.FontColor = palette.color(QPalette::Text);
    style.FontColorFaded = theme == GuiTheme::Dark ? QColor{140, 145, 152}
                                                   : QColor{100, 106, 115};

    // Connection points
    style.ConnectionPointColor = palette.color(QPalette::Mid);
    style.FilledConnectionPointColor = palette.color(QPalette::Highlight);

    // Shadow
    style.ShadowEnabled = false;

    // Warning and error: keep the pinned defaults, they have sufficient
    // contrast on both backgrounds.
    // Opacity: keep the pinned default (1.0).

    return style.toJson().toVariantMap();
}

} // namespace

QPointF snap_architecture_position(QPointF position) {
    return {std::round(position.x() / architecture_grid_step) * architecture_grid_step,
            std::round(position.y() / architecture_grid_step) * architecture_grid_step};
}

std::vector<QtTaskNodePresentation>
build_task_node_presentations(const ExperimentPresentationSnapshot& experiment, GuiTheme theme,
                              std::optional<ResourceId> highlighted_resource) {
    std::vector<QtTaskNodePresentation> result;
    result.reserve(experiment.tasks.size());
    for (const auto& task : experiment.tasks) {
        QtTaskNodePresentation row{.task_id = task.id,
                                   .resource_id = std::nullopt,
                                   .resource_name = QStringLiteral("Unassigned"),
                                   .accent = unassigned_accent_color(theme),
                                   .period = task.period,
                                   .deadline = task.deadline,
                                   .execution_time = std::nullopt,
                                   .assignment_valid = false,
                                   .highlighted = false};
        const auto assignment = find_assignment(experiment, task.id);
        if (assignment != nullptr) {
            row.resource_id = assignment->resource_id;
            row.accent = resource_accent_color(assignment->resource_id, theme);
            if (const auto* resource = find_resource(experiment, assignment->resource_id);
                resource != nullptr) {
                row.resource_name = QString::fromStdString(resource->name);
            }
            const auto profile = std::find_if(
                experiment.profiles.begin(), experiment.profiles.end(), [&](const auto& candidate) {
                    return candidate.task_id == task.id &&
                           candidate.resource_id == assignment->resource_id;
                });
            if (profile != experiment.profiles.end()) {
                row.execution_time = profile->execution_time;
                row.assignment_valid = true;
            }
        }
        row.highlighted =
            highlighted_resource.has_value() && row.resource_id == highlighted_resource;
        result.push_back(std::move(row));
    }
    return result;
}

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

void QtArchitectureGraphModel::rebuild(
    const GuiArchitectureGraph& graph,
    const std::vector<QtTaskNodePresentation>& task_presentations) {
    nodes_.clear();
    connections_.clear();
    connection_ids_.clear();
    std::size_t flat_index = 0;
    for (const auto& node : graph.nodes) {
        if (node.kind != GuiGraphNodeKind::Task) {
            continue;
        }
        const auto id = ids_.adapter_id(node.id);
        const auto position = QPointF{node.position.x, node.position.y};
        const auto presentation = std::find_if(
            task_presentations.begin(), task_presentations.end(), [&](const auto& candidate) {
                return candidate.task_id == std::get<TaskId>(node.entity);
            });
        nodes_.emplace(id,
                       NodeRecord{.entity = node.id,
                                  .caption = QString::fromStdString(node.label),
                                  .position = position,
                                  .size = QSize{230, 96},
                                  .input_count = 1,
                                  .output_count = 1,
                                  .presentation = presentation == task_presentations.end()
                                                      ? std::nullopt
                                                      : std::optional{*presentation}});
        ++flat_index;
    }

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
        const QtNodes::ConnectionId connection{*source, 0, *destination, 0};
        connections_.push_back(connection);
        if (edge.connection.has_value()) {
            connection_ids_.emplace_back(connection, edge.connection->id);
        }
    }
    Q_EMIT modelReset();
}

std::optional<GuiConnectionId>
QtArchitectureGraphModel::connection_for(const QtNodes::ConnectionId& connection_id) const {
    const auto found =
        std::find_if(connection_ids_.begin(), connection_ids_.end(),
                     [&](const auto& value) { return value.first == connection_id; });
    return found == connection_ids_.end() ? std::nullopt : std::optional{found->second};
}

std::optional<QtNodes::ConnectionId>
QtArchitectureGraphModel::connection_id_for(const GuiConnectionId& semantic_id) const {
    for (const auto& [qt_id, gui_id] : connection_ids_) {
        if (gui_id == semantic_id) {
            return qt_id;
        }
    }
    return std::nullopt;
}

std::optional<QtNodes::NodeId> QtArchitectureGraphModel::node_id_for(GuiGraphNodeId entity) const {
    const auto id = ids_.find_adapter_id(entity);
    return id.has_value() && nodes_.contains(*id) ? id : std::nullopt;
}

std::optional<GuiGraphNodeId> QtArchitectureGraphModel::entity_for(QtNodes::NodeId node_id) const {
    return nodes_.contains(node_id) ? ids_.entity_id(node_id) : std::nullopt;
}

std::vector<QRectF>
QtArchitectureGraphModel::occupied_rectangles(std::optional<GuiGraphNodeId> excluded) const {
    std::vector<QRectF> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        static_cast<void>(id);
        if (excluded == node.entity) {
            continue;
        }
        result.emplace_back(node.position, node.size);
    }
    return result;
}

const QtTaskNodePresentation*
QtArchitectureGraphModel::task_presentation(QtNodes::NodeId node_id) const {
    const auto found = nodes_.find(node_id);
    return found == nodes_.end() || !found->second.presentation.has_value()
               ? nullptr
               : &*found->second.presentation;
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

bool QtArchitectureGraphModel::connectionPossible(QtNodes::ConnectionId connection_id) const {
    // Callback-based guard: reject when structural editing is unavailable.
    if (!structural_edit_enabled_ || !structural_edit_enabled_()) {
        return false;
    }

    // Both nodes must exist
    const auto source_node = nodes_.find(connection_id.outNodeId);
    const auto dest_node = nodes_.find(connection_id.inNodeId);
    if (source_node == nodes_.end() || dest_node == nodes_.end()) {
        return false;
    }

    // Both entities must be tasks
    if (source_node->second.entity.kind != GuiGraphNodeKind::Task ||
        dest_node->second.entity.kind != GuiGraphNodeKind::Task) {
        return false;
    }

    // Source must be output port 0, destination must be input port 0
    if (connection_id.outPortIndex != 0 || connection_id.inPortIndex != 0) {
        return false;
    }

    // Self-loops: currently valid per domain policy (next_message_route includes them).
    // No explicit rejection here unless policy changes.

    // Duplicate ordered endpoint pair: reject if already exists
    if (connectionExists(connection_id)) {
        return false;
    }

    return true;
}

void QtArchitectureGraphModel::addConnection(QtNodes::ConnectionId connection_id) {
    // Validate again
    if (!connectionPossible(connection_id)) {
        return;
    }

    // Callback check
    if (!connection_create_requested_) {
        return;
    }

    // Map QtNodes node IDs to TaskIds
    const auto source_entity = entity_for(connection_id.outNodeId);
    const auto dest_entity = entity_for(connection_id.inNodeId);
    if (!source_entity.has_value() || !dest_entity.has_value()) {
        return;
    }

    const auto* source_node = nodes_.find(connection_id.outNodeId) != nodes_.end()
                                  ? &nodes_.at(connection_id.outNodeId)
                                  : nullptr;
    const auto* dest_node = nodes_.find(connection_id.inNodeId) != nodes_.end()
                                ? &nodes_.at(connection_id.inNodeId)
                                : nullptr;
    if (source_node == nullptr || dest_node == nullptr) {
        return;
    }

    const TaskId source_task{source_entity->entity_value};
    const TaskId dest_task{dest_entity->entity_value};

    // Invoke the creation callback exactly once.
    // The callback creates the route via the controller, which mutates the draft
    // and invokes bridge notification leading to rebuild.
    const bool created = connection_create_requested_(source_task, dest_task);

    if (created) {
        // The draft already has the route — do NOT insert into connections_ here.
        // The rebuild from draft will repopulate connections_ authoritatively.
        // No connectionCreated signal is needed because the synchronous callback
        // (edits_.create_connection → apply → RestoreDraftCommand::redo →
        // bridge_.restore_draft → draftChanged → refresh → model_.rebuild →
        // modelReset → BasicGraphicsScene::onModelReset) already clears and
        // re-populates all ConnectionGraphicsObjects from the updated model
        // state.  Emitting connectionCreated here would overwrite the
        // already-placed authoritative graphics object with a redundant one.
    }
}

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
    case QtNodes::NodeRole::ValidationState:
        if (node.presentation.has_value() && !node.presentation->assignment_valid) {
            return QVariant::fromValue(QtNodes::NodeValidationState{
                QtNodes::NodeValidationState::State::Error,
                QStringLiteral("Task is unassigned or lacks an execution profile")});
        }
        return QVariant::fromValue(QtNodes::NodeValidationState{});
    case QtNodes::NodeRole::LabelVisible:
        return false;
    case QtNodes::NodeRole::Label:
        return QString{};
    case QtNodes::NodeRole::Style:
        return build_theme_aware_node_style(current_workbench_theme());
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
    const auto found = nodes_.find(node_id);
    if (found == nodes_.end()) {
        return {};
    }
    const auto count = port_type == QtNodes::PortType::In ? found->second.input_count
                                                          : found->second.output_count;
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
        return true;
    case QtNodes::PortRole::Caption:
        return port_caption(port_type, count, index);
    default:
        return {};
    }
}

bool QtArchitectureGraphModel::setPortData(QtNodes::NodeId, QtNodes::PortType, QtNodes::PortIndex,
                                           const QVariant&, QtNodes::PortRole) {
    return false;
}

bool QtArchitectureGraphModel::deleteConnection(QtNodes::ConnectionId connection_id) {
    if (!connection_delete_requested_) {
        return false;
    }

    // Resolve the domain connection identity
    const auto gui_connection = connection_for(connection_id);
    if (!gui_connection.has_value()) {
        // Non-domain edges (logical, decorative) — reject cleanly
        return false;
    }

    // Only editable communication routes may be deleted through this path
    if (gui_connection->kind != GuiConnectionKind::Communication) {
        return false;
    }

    // Invoke the deletion callback — it removes the route via the controller
    const bool removed = connection_delete_requested_(*gui_connection);

    if (removed) {
        // Do NOT erase from connections_ here — rely on domain rebuild.
        // The rebuild from draft will update connections_ authoritatively.
        // No connectionDeleted signal is needed because the synchronous callback
        // already triggers model_.rebuild() → modelReset, which clears and
        // re-populates all ConnectionGraphicsObjects from the updated model
        // state (which no longer contains this connection).
    }

    return removed;
}

bool QtArchitectureGraphModel::deleteNode(QtNodes::NodeId) { return false; }

} // namespace cpssim::qt
