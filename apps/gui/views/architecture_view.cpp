/***
 * File: apps/gui/views/architecture_view.cpp
 * Purpose: Render and interact with the deterministic G04 architecture graph.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "architecture_view.hpp"

#include "cpssim/gui/display_scale.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace cpssim::gui {
namespace {

constexpr float minimum_zoom = 0.025F;
constexpr float maximum_zoom = 2.5F;
constexpr float route_hit_radius = 7.0F;

struct ScreenTransform {
    ImVec2 origin;
    float pan_x;
    float pan_y;
    float zoom;

    ImVec2 point(GuiLogicalPoint value) const {
        return {origin.x + pan_x + value.x * zoom, origin.y + pan_y + value.y * zoom};
    }
};

struct ScreenRect {
    ImVec2 minimum;
    ImVec2 maximum;
};

ScreenRect node_rect(const GuiGraphNode& node, const ScreenTransform& transform) {
    const auto minimum = transform.point(node.position);
    return {.minimum = minimum,
            .maximum = {minimum.x + node.size.width * transform.zoom,
                        minimum.y + node.size.height * transform.zoom}};
}

bool contains(const ScreenRect& rect, ImVec2 point) {
    return point.x >= rect.minimum.x && point.x <= rect.maximum.x && point.y >= rect.minimum.y &&
           point.y <= rect.maximum.y;
}

bool intersects(const ScreenRect& left, const ScreenRect& right) {
    return left.maximum.x >= right.minimum.x && left.minimum.x <= right.maximum.x &&
           left.maximum.y >= right.minimum.y && left.minimum.y <= right.maximum.y;
}

ImVec2 center(const ScreenRect& rect) {
    return {(rect.minimum.x + rect.maximum.x) * 0.5F, (rect.minimum.y + rect.maximum.y) * 0.5F};
}

const GuiGraphNode* hit_node(const GuiArchitectureGraph& graph, const ScreenTransform& transform,
                             ImVec2 mouse, GuiGraphNodeKind kind) {
    for (auto node = graph.nodes.rbegin(); node != graph.nodes.rend(); ++node) {
        if (node->kind == kind && contains(node_rect(*node, transform), mouse)) {
            return &*node;
        }
    }
    return nullptr;
}

std::pair<ImVec2, ImVec2> edge_points(const GuiArchitectureGraph& graph, const GuiGraphEdge& edge,
                                      const ScreenTransform& transform) {
    const auto* source = find_graph_node(graph, edge.source);
    const auto* destination = find_graph_node(graph, edge.destination);
    if (source == nullptr || destination == nullptr) {
        return {};
    }
    const auto source_rect = node_rect(*source, transform);
    const auto destination_rect = node_rect(*destination, transform);
    if (edge.kind == GuiGraphEdgeKind::Assignment) {
        const auto task_center = center(source_rect);
        return {{task_center.x, source_rect.minimum.y},
                {task_center.x, destination_rect.minimum.y + 28.0F * transform.zoom}};
    }

    const auto source_center = center(source_rect);
    const auto destination_center = center(destination_rect);
    if (source_center.x <= destination_center.x) {
        return {{source_rect.maximum.x, source_center.y},
                {destination_rect.minimum.x, destination_center.y}};
    }
    return {{source_rect.minimum.x, source_center.y},
            {destination_rect.maximum.x, destination_center.y}};
}

std::array<std::pair<ImVec2, ImVec2>, 3>
edge_segments(const GuiArchitectureGraph& graph, const GuiGraphEdge& edge,
              const ScreenTransform& transform) {
    const auto [start, finish] = edge_points(graph, edge, transform);
    const auto middle_x = (start.x + finish.x) * 0.5F;
    const ImVec2 first_bend{middle_x, start.y};
    const ImVec2 second_bend{middle_x, finish.y};
    return {{{start, first_bend}, {first_bend, second_bend}, {second_bend, finish}}};
}

float distance_to_segment(ImVec2 point, ImVec2 start, ImVec2 finish) {
    const auto dx = finish.x - start.x;
    const auto dy = finish.y - start.y;
    const auto length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.0F) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }
    const auto projection = std::clamp(
        ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared, 0.0F, 1.0F);
    return std::hypot(point.x - (start.x + projection * dx), point.y - (start.y + projection * dy));
}

const GuiGraphEdge* hit_relation(const GuiArchitectureGraph& graph,
                                 const ScreenTransform& transform, ImVec2 mouse) {
    for (const auto& edge : graph.edges) {
        if (edge.kind == GuiGraphEdgeKind::Assignment) {
            continue;
        }
        const auto segments = edge_segments(graph, edge, transform);
        if (std::any_of(segments.begin(), segments.end(), [mouse](const auto& segment) {
                return distance_to_segment(mouse, segment.first, segment.second) <=
                       route_hit_radius;
            })) {
            return &edge;
        }
    }
    return nullptr;
}

void draw_dashed_line(ImDrawList* draw_list, ImVec2 start, ImVec2 finish, ImU32 color,
                      float thickness, float dash_length, float gap_length) {
    const auto dx = finish.x - start.x;
    const auto dy = finish.y - start.y;
    const auto length = std::hypot(dx, dy);
    if (length <= 0.0F) {
        return;
    }
    const auto unit_x = dx / length;
    const auto unit_y = dy / length;
    for (float offset = 0.0F; offset < length; offset += dash_length + gap_length) {
        const auto end = std::min(offset + dash_length, length);
        draw_list->AddLine({start.x + unit_x * offset, start.y + unit_y * offset},
                           {start.x + unit_x * end, start.y + unit_y * end}, color, thickness);
    }
}

void draw_arrow(ImDrawList* draw_list, ImVec2 start, ImVec2 finish, ImU32 color, float scale) {
    const auto angle = std::atan2(finish.y - start.y, finish.x - start.x);
    const auto length = 9.0F * scale;
    constexpr float spread = 0.55F;
    const ImVec2 left{finish.x - length * std::cos(angle - spread),
                      finish.y - length * std::sin(angle - spread)};
    const ImVec2 right{finish.x - length * std::cos(angle + spread),
                       finish.y - length * std::sin(angle + spread)};
    draw_list->AddTriangleFilled(finish, left, right, color);
}

bool route_related_to_selected_task(const GuiGraphEdge& edge,
                                    const StructuralSelection& selection) {
    return edge.route_reference.has_value() && selection.task_id().has_value() &&
           (edge.route_reference->source_task_id == *selection.task_id() ||
            edge.route_reference->destination_task_id == *selection.task_id());
}

bool resource_is_selected_assignment(const ExperimentPresentationSnapshot& experiment,
                                     const StructuralSelection& selection,
                                     ResourceId resource_id) {
    if (!selection.task_id().has_value()) {
        return false;
    }
    const auto* assignment = find_assignment(experiment, *selection.task_id());
    return assignment != nullptr && assignment->resource_id == resource_id;
}

std::optional<ResourceId> active_assignment(const ExperimentPresentationSnapshot& experiment,
                                            TaskId task_id) {
    const auto* assignment = find_assignment(experiment, task_id);
    return assignment != nullptr ? std::optional<ResourceId>{assignment->resource_id}
                                 : std::nullopt;
}

std::optional<ResourceId> draft_assignment(const std::vector<DraftTaskAssignment>& assignments,
                                           TaskId task_id) {
    const auto found = std::find_if(assignments.begin(), assignments.end(),
                                    [task_id](const auto& row) { return row.task_id == task_id; });
    return found == assignments.end() ? std::nullopt : found->resource_id;
}

void draw_edges(ImDrawList* draw_list, const GuiArchitectureGraph& graph,
                const ScreenTransform& transform, const StructuralSelection& selection) {
    for (const auto& edge : graph.edges) {
        const auto segments = edge_segments(graph, edge, transform);
        const auto finish = segments.back().second;
        if (edge.kind == GuiGraphEdgeKind::MessageRoute) {
            const auto selected = edge.connection.has_value() &&
                                  selection.connection() == edge.connection->id;
            const auto related = route_related_to_selected_task(edge, selection);
            const auto color = ImGui::GetColorU32(selected || related ? ImGuiCol_PlotLinesHovered
                                                                      : ImGuiCol_PlotLines);
            const auto thickness = selected ? 3.0F : (related ? 2.2F : 1.4F);
            for (const auto& segment : segments)
                draw_dashed_line(draw_list, segment.first, segment.second, color, thickness,
                                 std::max(10.0F * transform.zoom, 4.0F),
                                 std::max(6.0F * transform.zoom, 3.0F));
            draw_arrow(draw_list, segments.back().first, finish, color,
                       std::max(transform.zoom, 0.65F));
        } else if (edge.kind == GuiGraphEdgeKind::FunctionalDependency) {
            const auto related = selection.task_id().has_value() &&
                                 (edge.source.entity_value == selection.task_id()->value() ||
                                  edge.destination.entity_value == selection.task_id()->value());
            const auto color =
                ImGui::GetColorU32(related ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotLines);
            for (const auto& segment : segments)
                draw_list->AddLine(segment.first, segment.second, color,
                                   related ? 2.5F : 1.8F);
            draw_arrow(draw_list, segments.back().first, finish, color,
                       std::max(transform.zoom, 0.65F));
        } else {
            continue;
        }
    }
}

void draw_resource_node(ImDrawList* draw_list, const GuiGraphNode& node,
                        const ScreenTransform& transform,
                        const ExperimentPresentationSnapshot& experiment,
                        const StructuralSelection& selection,
                        std::optional<ResourceId> drag_target,
                        bool valid_drag_target) {
    const auto rect = node_rect(node, transform);
    const auto resource_id = std::get<ResourceId>(node.entity);
    const auto selected = selection.resource_id() == resource_id ||
                          resource_is_selected_assignment(experiment, selection, resource_id);
    auto border = ImGui::GetColorU32(selected ? ImGuiCol_HeaderHovered : ImGuiCol_Border);
    if (drag_target == resource_id) {
        border =
            ImGui::GetColorU32(valid_drag_target ? ImGuiCol_CheckMark : ImGuiCol_PlotHistogram);
    }
    draw_list->AddRectFilled(rect.minimum, rect.maximum, ImGui::GetColorU32(ImGuiCol_ChildBg),
                             10.0F * transform.zoom);
    draw_list->AddRect(rect.minimum, rect.maximum, border, 10.0F * transform.zoom, 0,
                       selected || drag_target == resource_id ? 3.0F : 1.4F);
    draw_list->AddLine({rect.minimum.x, rect.minimum.y + 28.0F * transform.zoom},
                       {rect.maximum.x, rect.minimum.y + 28.0F * transform.zoom}, border, 1.2F);
    draw_list->AddText(
        nullptr, sanitize_gui_font_size(ImGui::GetFontSize() * transform.zoom),
        {rect.minimum.x + 8.0F * transform.zoom, rect.minimum.y + 5.0F * transform.zoom},
        ImGui::GetColorU32(ImGuiCol_Text), node.label.c_str());
}

void draw_task_node(ImDrawList* draw_list, const GuiGraphNode& node,
                    const ScreenTransform& transform,
                    const ExperimentPresentationSnapshot& experiment,
                    const std::vector<DraftTaskAssignment>& assignments,
                    const StructuralSelection& selection,
                    bool dragging, bool read_only_preview) {
    const auto rect = node_rect(node, transform);
    const auto task_id = std::get<TaskId>(node.entity);
    const auto selected = selection.task_id() == task_id;
    const auto draft_resource =
        read_only_preview ? std::optional<ResourceId>{} : draft_assignment(assignments, task_id);
    const auto draft_valid =
        read_only_preview || (draft_resource.has_value() &&
                              graph_assignment_accessible(experiment, task_id, *draft_resource));
    const auto active_resource = active_assignment(experiment, task_id);
    const auto draft_changed = draft_resource != active_resource;

    auto fill = ImGui::GetColorU32(ImGuiCol_FrameBg);
    if (dragging) {
        fill = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    }
    draw_list->AddRectFilled(rect.minimum, rect.maximum, fill, 3.0F * transform.zoom);
    draw_list->AddRect(rect.minimum, rect.maximum,
                       ImGui::GetColorU32(selected ? ImGuiCol_HeaderHovered : ImGuiCol_Border),
                       3.0F * transform.zoom, 0, selected ? 3.0F : 1.5F);
    const auto text_position =
        ImVec2{rect.minimum.x + 8.0F * transform.zoom, rect.minimum.y + 5.0F * transform.zoom};
    const auto clip = ImVec4{rect.minimum.x + 6.0F * transform.zoom, rect.minimum.y,
                             rect.maximum.x - 6.0F * transform.zoom, rect.maximum.y};
    draw_list->AddText(nullptr, sanitize_gui_font_size(ImGui::GetFontSize() * transform.zoom),
                       text_position, ImGui::GetColorU32(ImGuiCol_Text), node.label.c_str(),
                       nullptr, 0.0F, &clip);
    const auto port_radius = std::max(3.0F * transform.zoom, 2.0F);
    const ImVec2 input{rect.minimum.x, (rect.minimum.y + rect.maximum.y) * 0.5F};
    const ImVec2 output{rect.maximum.x, (rect.minimum.y + rect.maximum.y) * 0.5F};
    draw_list->AddCircle(input, port_radius, ImGui::GetColorU32(ImGuiCol_PlotLines), 12,
                         std::max(1.0F, transform.zoom));
    draw_list->AddCircleFilled(output, port_radius, ImGui::GetColorU32(ImGuiCol_PlotLines), 12);
    if (draft_changed && draft_resource.has_value()) {
        const auto draft_label = "Resource R" + std::to_string(draft_resource->value());
        draw_list->AddText(
            nullptr, sanitize_gui_font_size(ImGui::GetFontSize() * 0.82F * transform.zoom),
            {rect.minimum.x + 10.0F * transform.zoom, rect.maximum.y - 21.0F * transform.zoom},
            ImGui::GetColorU32(ImGuiCol_PlotHistogram), draft_label.c_str());
    }
    if (!draft_valid && !read_only_preview) {
        const auto warning = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
        const ImVec2 top{rect.maximum.x - 9.0F * transform.zoom,
                         rect.minimum.y + 7.0F * transform.zoom};
        draw_list->AddTriangleFilled(
            top, {top.x - 8.0F * transform.zoom, top.y + 15.0F * transform.zoom},
            {top.x + 8.0F * transform.zoom, top.y + 15.0F * transform.zoom}, warning);
    }
}

void fit_graph(ArchitectureViewState& state, const GuiArchitectureGraph& graph,
               GuiArchitectureWorkspace& layout, ImVec2 canvas_size) {
    constexpr float padding = 32.0F;
    const auto available_width = std::max(canvas_size.x - 2.0F * padding, 1.0F);
    const auto available_height = std::max(canvas_size.y - 2.0F * padding, 1.0F);
    layout.zoom = std::clamp(std::min(available_width / graph.logical_size.width,
                                     available_height / graph.logical_size.height),
                            minimum_zoom, maximum_zoom);
    layout.pan_x = (canvas_size.x - graph.logical_size.width * layout.zoom) * 0.5F;
    layout.pan_y = (canvas_size.y - graph.logical_size.height * layout.zoom) * 0.5F;
    state.fit_requested = false;
}

void update_zoom(GuiArchitectureWorkspace& layout, ImVec2 canvas_origin, ImVec2 mouse, float wheel) {
    const auto old_zoom = layout.zoom;
    const auto factor = wheel > 0.0F ? 1.12F : (1.0F / 1.12F);
    layout.zoom = std::clamp(old_zoom * factor, minimum_zoom, maximum_zoom);
    const auto logical_x = (mouse.x - canvas_origin.x - layout.pan_x) / old_zoom;
    const auto logical_y = (mouse.y - canvas_origin.y - layout.pan_y) / old_zoom;
    layout.pan_x = mouse.x - canvas_origin.x - logical_x * layout.zoom;
    layout.pan_y = mouse.y - canvas_origin.y - logical_y * layout.zoom;
}

void reject_drop(ArchitectureViewState& state, TaskId task_id, std::string reason) {
    state.status = "Cannot assign T" + std::to_string(task_id.value()) + ": " + std::move(reason);
    state.status_error = true;
}

} // namespace

bool draw_architecture_view(const GuiArchitectureGraph& graph,
                            const ExperimentPresentationSnapshot& experiment,
                            std::vector<DraftTaskAssignment>& assignments,
                            StructuralSelection& selection, GuiArchitectureWorkspace& layout,
                            ArchitectureViewState& state, bool editing_enabled,
                            bool read_only_preview, GuiPointerRegionMap* pointer_regions) {
    if (read_only_preview) {
        state.pressed_task.reset();
        state.dragging_task = false;
    }
    if (ImGui::Button("Fit to view")) {
        state.fit_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto Layout")) {
        auto_layout_architecture(layout);
        state.fit_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        reset_architecture_layout(layout);
        state.fit_requested = true;
    }
    ImGui::SameLine();
    int mode = static_cast<int>(layout.mode);
    ImGui::SetNextItemWidth(7.0F * ImGui::GetFontSize());
    if (ImGui::Combo("Mode", &mode, "Select\0Arrange\0Assign\0")) {
        layout.mode = static_cast<GuiArchitectureMode>(mode);
    }
    if (layout.mode == GuiArchitectureMode::Arrange && selection.task_id().has_value()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Position")) {
            reset_task_layout_position(layout, *selection.task_id());
        }
    } else if (layout.mode == GuiArchitectureMode::Arrange &&
               selection.resource_id().has_value()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Auto Size Resource")) {
            reset_resource_layout_size(layout, *selection.resource_id());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Position")) {
            reset_resource_layout_position(layout, *selection.resource_id());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Size")) {
            reset_resource_layout_size(layout, *selection.resource_id());
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled(read_only_preview
                            ? "Zoom %.0f%% | wheel: zoom | middle-drag: pan | read-only preview"
                            : "Zoom %.0f%% | wheel: zoom | middle-drag: pan",
                        layout.zoom * 100.0F);
    ImGui::SameLine();
    ImGui::TextDisabled("| solid: functional | dashed: network | dim: assignment");
    if (!state.status.empty()) {
        const auto color = state.status_error ? ImVec4{1.0F, 0.48F, 0.32F, 1.0F}
                                              : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::TextColored(color, "%s", state.status.c_str());
    }

    auto canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.x = std::max(canvas_size.x, 50.0F);
    canvas_size.y = std::max(canvas_size.y, 50.0F);
    const auto canvas_origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Architecture canvas input", canvas_size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const auto canvas_hovered = ImGui::IsItemHovered();
    const auto mouse = ImGui::GetIO().MousePos;

    if (state.fit_requested) {
        fit_graph(state, graph, layout, canvas_size);
    }
    if (canvas_hovered && ImGui::GetIO().MouseWheel != 0.0F) {
        update_zoom(layout, canvas_origin, mouse, ImGui::GetIO().MouseWheel);
    }
    if (canvas_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
        layout.pan_x += ImGui::GetIO().MouseDelta.x;
        layout.pan_y += ImGui::GetIO().MouseDelta.y;
    }

    const ScreenTransform transform{
        .origin = canvas_origin, .pan_x = layout.pan_x, .pan_y = layout.pan_y, .zoom = layout.zoom};
    if (pointer_regions != nullptr) {
        pointer_regions->add(
            {ImGui::GetID("Architecture passive canvas"),
             {canvas_origin.x, canvas_origin.y, canvas_origin.x + canvas_size.x,
              canvas_origin.y + canvas_size.y},
             GuiPointerRegionBehavior::Passive});
        for (const auto& edge : graph.edges) {
            if (!edge.connection.has_value()) {
                continue;
            }
            constexpr float tolerance = 8.0F;
            const auto identity = "Architecture connection " +
                                  std::to_string(static_cast<int>(edge.connection->id.kind)) + " " +
                                  std::to_string(edge.source.entity_value) + " " +
                                  std::to_string(edge.destination.entity_value);
            constexpr int segments = 12;
            const auto region_identity = ImGui::GetID(identity.c_str());
            for (const auto& route_segment : edge_segments(graph, edge, transform)) {
                for (int segment = 0; segment < segments; ++segment) {
                    const auto first = static_cast<float>(segment) / segments;
                    const auto second = static_cast<float>(segment + 1) / segments;
                    const ImVec2 segment_start{
                        route_segment.first.x +
                            (route_segment.second.x - route_segment.first.x) * first,
                        route_segment.first.y +
                            (route_segment.second.y - route_segment.first.y) * first};
                    const ImVec2 segment_end{
                        route_segment.first.x +
                            (route_segment.second.x - route_segment.first.x) * second,
                        route_segment.first.y +
                            (route_segment.second.y - route_segment.first.y) * second};
                    pointer_regions->add(
                        {region_identity,
                         {std::min(segment_start.x, segment_end.x) - tolerance,
                          std::min(segment_start.y, segment_end.y) - tolerance,
                          std::max(segment_start.x, segment_end.x) + tolerance,
                          std::max(segment_start.y, segment_end.y) + tolerance},
                         GuiPointerRegionBehavior::BoundarySensitive});
                }
            }
        }
        for (const auto& node : graph.nodes) {
            const auto rect = node_rect(node, transform);
            const auto identity = "Architecture node " +
                                  std::to_string(static_cast<int>(node.kind)) + " " +
                                  std::to_string(node.id.entity_value);
            pointer_regions->add(
                {ImGui::GetID(identity.c_str()),
                 {rect.minimum.x, rect.minimum.y, rect.maximum.x, rect.maximum.y},
                 GuiPointerRegionBehavior::BoundarySensitive});
            if (node.kind == GuiGraphNodeKind::Resource) {
                constexpr float handle = 8.0F;
                const auto handle_identity = ImGui::GetID((identity + " resize").c_str());
                pointer_regions->add(
                    {handle_identity,
                     {rect.maximum.x - handle, rect.minimum.y, rect.maximum.x + handle,
                      rect.maximum.y + handle},
                     GuiPointerRegionBehavior::DragHandle});
                pointer_regions->add(
                    {handle_identity,
                     {rect.minimum.x, rect.maximum.y - handle, rect.maximum.x + handle,
                      rect.maximum.y + handle},
                     GuiPointerRegionBehavior::DragHandle});
            }
        }
    }
    bool structural_selection_changed = false;
    if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto* task = hit_node(graph, transform, mouse, GuiGraphNodeKind::Task);
        const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
        const auto* route = hit_relation(graph, transform, mouse);
        if (task != nullptr) {
            select_graph_node(selection, *task);
            structural_selection_changed = true;
            if (!read_only_preview && editing_enabled &&
                layout.mode != GuiArchitectureMode::Select) {
                state.pressed_task = std::get<TaskId>(task->entity);
            }
        } else if (route != nullptr) {
            select_graph_edge(selection, *route);
            structural_selection_changed = true;
            state.status = route->kind == GuiGraphEdgeKind::FunctionalDependency
                               ? "Selected functional dependency (presentation only; no messages)"
                               : "Selected network route (configured message delay)";
            state.status_error = false;
        } else if (resource != nullptr) {
            select_graph_node(selection, *resource);
            structural_selection_changed = true;
            if (!read_only_preview && editing_enabled &&
                layout.mode == GuiArchitectureMode::Arrange) {
                state.pressed_resource = std::get<ResourceId>(resource->entity);
                const auto rect = node_rect(*resource, transform);
                constexpr float handle = 9.0F;
                state.resizing_resource_x = std::abs(mouse.x - rect.maximum.x) <= handle;
                state.resizing_resource_y = std::abs(mouse.y - rect.maximum.y) <= handle;
            }
        }
    }

    if (canvas_hovered) {
        if (const auto* hovered_task =
                hit_node(graph, transform, mouse, GuiGraphNodeKind::Task);
            hovered_task != nullptr &&
            ImGui::CalcTextSize(hovered_task->label.c_str()).x + 16.0F >
                hovered_task->size.width * transform.zoom) {
            ImGui::SetTooltip("%s", hovered_task->label.c_str());
        } else if (const auto* hovered = hit_relation(graph, transform, mouse);
            hovered != nullptr && hovered->connection.has_value()) {
            const auto& connection = *hovered->connection;
            ImGui::BeginTooltip();
            ImGui::Text("%s connection", connection.id.kind == GuiConnectionKind::Logical
                                              ? "Logical"
                                              : "Communication");
            ImGui::Text("T%llu -> T%llu",
                        static_cast<unsigned long long>(connection.id.source_task_id.value()),
                        static_cast<unsigned long long>(connection.id.destination_task_id.value()));
            ImGui::Text("Displayed latency: %lld ticks",
                        static_cast<long long>(connection.displayed_latency));
            ImGui::TextDisabled("%s", connection.creates_network_events
                                         ? "Creates canonical message events"
                                         : "Presentation-only; creates no network events");
            ImGui::EndTooltip();
        }
    }

    if (!read_only_preview && state.pressed_task.has_value() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        state.dragging_task = true;
        if (layout.mode == GuiArchitectureMode::Arrange && editing_enabled) {
            if (const auto* node =
                    find_graph_node(graph, task_graph_node_id(*state.pressed_task));
                node != nullptr) {
                set_task_layout_position(
                    layout, *state.pressed_task,
                    {node->position.x + ImGui::GetIO().MouseDelta.x / layout.zoom,
                     node->position.y + ImGui::GetIO().MouseDelta.y / layout.zoom});
            }
        }
    }
    if (state.pressed_resource.has_value() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
        editing_enabled && layout.mode == GuiArchitectureMode::Arrange) {
        const auto* node =
            find_graph_node(graph, resource_graph_node_id(*state.pressed_resource));
        if (node != nullptr) {
            const auto dx = ImGui::GetIO().MouseDelta.x / layout.zoom;
            const auto dy = ImGui::GetIO().MouseDelta.y / layout.zoom;
            if (state.resizing_resource_x || state.resizing_resource_y) {
                set_resource_layout_size(
                    layout, *state.pressed_resource,
                    {std::max(48.0F, node->size.width + (state.resizing_resource_x ? dx : 0.0F)),
                     std::max(36.0F,
                              node->size.height + (state.resizing_resource_y ? dy : 0.0F))});
            } else {
                set_resource_layout_position(layout, *state.pressed_resource,
                                             {node->position.x + dx, node->position.y + dy});
                for (const auto& assignment : experiment.assignments) {
                    if (assignment.resource_id != *state.pressed_resource) continue;
                    if (const auto* task =
                            find_graph_node(graph, task_graph_node_id(assignment.task_id));
                        task != nullptr)
                        set_task_layout_position(layout, assignment.task_id,
                                                 {task->position.x + dx, task->position.y + dy});
                }
            }
        }
    }
    if (state.pressed_task.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const auto task_id = *state.pressed_task;
        if (state.dragging_task && layout.mode == GuiArchitectureMode::Assign) {
            const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
            if (resource == nullptr) {
                reject_drop(state, task_id, "drop onto a resource container");
            } else {
                const auto resource_id = std::get<ResourceId>(resource->entity);
                if (!editing_enabled) {
                    reject_drop(state, task_id,
                                "the run is Running; pause it before editing the draft");
                } else if (!graph_assignment_accessible(experiment, task_id, resource_id)) {
                    reject_drop(state, task_id,
                                "resource R" + std::to_string(resource_id.value()) +
                                    " has no configured execution profile for this task");
                } else {
                    const auto assignment =
                        std::find_if(assignments.begin(), assignments.end(),
                                     [task_id](const auto& row) { return row.task_id == task_id; });
                    if (assignment != assignments.end()) assignment->resource_id = resource_id;
                    else assignments.push_back({task_id, resource_id});
                    state.status = "Pending assignment updated: T" +
                                   std::to_string(task_id.value()) + " -> R" +
                                   std::to_string(resource_id.value()) +
                                   ". Use Apply and restart to activate it.";
                    state.status_error = false;
                }
            }
        }
        state.pressed_task.reset();
        state.dragging_task = false;
    }
    if (state.pressed_resource.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        state.pressed_resource.reset();
        state.resizing_resource_x = false;
        state.resizing_resource_y = false;
    }

    std::optional<ResourceId> drag_target;
    bool valid_drag_target = false;
    if (state.dragging_task && layout.mode == GuiArchitectureMode::Assign) {
        const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
        if (resource != nullptr) {
            drag_target = std::get<ResourceId>(resource->entity);
            valid_drag_target = editing_enabled && layout.mode == GuiArchitectureMode::Assign &&
                graph_assignment_accessible(experiment, *state.pressed_task, *drag_target);
        }
    }

    auto* draw_list = ImGui::GetWindowDrawList();
    const ScreenRect canvas_rect{
        .minimum = canvas_origin,
        .maximum = {canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y}};
    draw_list->PushClipRect(canvas_rect.minimum, canvas_rect.maximum, true);
    draw_list->AddRectFilled(canvas_rect.minimum, canvas_rect.maximum,
                             ImGui::GetColorU32(ImGuiCol_WindowBg));
    for (const auto& node : graph.nodes) {
        if (node.kind != GuiGraphNodeKind::Resource ||
            !intersects(node_rect(node, transform), canvas_rect)) {
            continue;
        }
        draw_resource_node(draw_list, node, transform, experiment, selection, drag_target,
                           valid_drag_target);
    }
    draw_edges(draw_list, graph, transform, selection);
    for (const auto& node : graph.nodes) {
        if (node.kind != GuiGraphNodeKind::Task ||
            !intersects(node_rect(node, transform), canvas_rect)) {
            continue;
        }
        const auto dragging =
            state.dragging_task && state.pressed_task == std::get<TaskId>(node.entity);
        draw_task_node(draw_list, node, transform, experiment, assignments, selection, dragging,
                       read_only_preview);
    }
    if (state.dragging_task && layout.mode == GuiArchitectureMode::Assign) {
        draw_list->AddLine(
            mouse, {mouse.x + 20.0F, mouse.y + 20.0F},
            ImGui::GetColorU32(valid_drag_target ? ImGuiCol_CheckMark : ImGuiCol_PlotHistogram),
            3.0F);
    }
    draw_list->PopClipRect();
    return structural_selection_changed;
}

} // namespace cpssim::gui
