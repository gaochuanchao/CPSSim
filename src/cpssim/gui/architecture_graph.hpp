/***
 * File: src/cpssim/gui/architecture_graph.hpp
 * Purpose: Declare the deterministic ImGui-independent G04 architecture graph.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Stable graph identities are derived only from strong simulator IDs.
 ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cpssim {

enum class GuiGraphNodeKind {
    Task,
    Resource,
};

enum class GuiGraphEdgeKind {
    MessageRoute,
    Assignment,
};

/*** Identifies one graph node without depending on labels or vector positions. ***/
struct GuiGraphNodeId {
    GuiGraphNodeKind kind;
    std::uint64_t entity_value;

    auto operator<=>(const GuiGraphNodeId&) const = default;
};

/*** Identifies one graph edge by its typed relation and stable endpoints. ***/
struct GuiGraphEdgeId {
    GuiGraphEdgeKind kind;
    GuiGraphNodeId source;
    GuiGraphNodeId destination;

    auto operator<=>(const GuiGraphEdgeId&) const = default;
};

struct GuiLogicalPoint {
    float x;
    float y;

    bool operator==(const GuiLogicalPoint&) const = default;
};

struct GuiLogicalSize {
    float width;
    float height;

    bool operator==(const GuiLogicalSize&) const = default;
};

using GuiGraphEntity = std::variant<TaskId, ResourceId>;

/*** Stores one task rectangle or resource container in logical coordinates. ***/
struct GuiGraphNode {
    GuiGraphNodeId id;
    GuiGraphNodeKind kind;
    GuiGraphEntity entity;
    std::string label;
    GuiLogicalPoint position;
    GuiLogicalSize size;

    bool operator==(const GuiGraphNode&) const = default;
};

/*** Stores one configured route or applied placement relation. ***/
struct GuiGraphEdge {
    GuiGraphEdgeId id;
    GuiGraphEdgeKind kind;
    GuiGraphNodeId source;
    GuiGraphNodeId destination;
    std::optional<GuiRouteIdentity> route_reference;
    std::optional<GuiTaskAssignmentPresentation> assignment_reference;

    bool operator==(const GuiGraphEdge&) const = default;
};

/*** Owns a complete deterministic logical graph detached from simulator state. ***/
struct GuiArchitectureGraph {
    std::vector<GuiGraphNode> nodes;
    std::vector<GuiGraphEdge> edges;
    GuiLogicalSize logical_size;

    bool operator==(const GuiArchitectureGraph&) const = default;
};

GuiGraphNodeId task_graph_node_id(TaskId task_id);
GuiGraphNodeId resource_graph_node_id(ResourceId resource_id);

/*** Builds canonical nodes, relations, and cycle-safe logical layout. ***/
GuiArchitectureGraph build_architecture_graph(const ExperimentPresentationSnapshot& experiment);

const GuiGraphNode* find_graph_node(const GuiArchitectureGraph& graph, GuiGraphNodeId node_id);

/*** Reports whether a draft task placement has a configured execution profile. ***/
bool graph_assignment_accessible(const ExperimentPresentationSnapshot& experiment, TaskId task_id,
                                 ResourceId resource_id);

/*** Maps graph entities back into the workbench's shared strong-ID selection. ***/
void select_graph_node(GuiSelection& selection, const GuiGraphNode& node);
void select_graph_edge(GuiSelection& selection, const GuiGraphEdge& edge);

} // namespace cpssim
