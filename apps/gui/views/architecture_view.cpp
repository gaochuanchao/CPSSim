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

const GuiGraphEdge* hit_route(const GuiArchitectureGraph& graph, const ScreenTransform& transform,
                              ImVec2 mouse) {
    for (const auto& edge : graph.edges) {
        if (edge.kind != GuiGraphEdgeKind::MessageRoute) {
            continue;
        }
        const auto [start, finish] = edge_points(graph, edge, transform);
        if (distance_to_segment(mouse, start, finish) <= route_hit_radius) {
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

bool route_related_to_selected_task(const GuiGraphEdge& edge, const GuiSelection& selection) {
    return edge.route_reference.has_value() && selection.task_id().has_value() &&
           (edge.route_reference->source_task_id == *selection.task_id() ||
            edge.route_reference->destination_task_id == *selection.task_id());
}

bool assignment_related_to_selected_task(const GuiGraphEdge& edge, const GuiSelection& selection) {
    return edge.assignment_reference.has_value() && selection.task_id().has_value() &&
           edge.assignment_reference->task_id == *selection.task_id();
}

bool resource_is_selected_assignment(const ExperimentPresentationSnapshot& experiment,
                                     const GuiSelection& selection, ResourceId resource_id) {
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

void draw_edges(ImDrawList* draw_list, const GuiArchitectureGraph& graph,
                const ScreenTransform& transform, const GuiSelection& selection) {
    for (const auto& edge : graph.edges) {
        const auto [start, finish] = edge_points(graph, edge, transform);
        if (edge.kind == GuiGraphEdgeKind::MessageRoute) {
            const auto selected = selection.route_id() == edge.route_reference;
            const auto related = route_related_to_selected_task(edge, selection);
            const auto color = ImGui::GetColorU32(selected || related ? ImGuiCol_PlotLinesHovered
                                                                      : ImGuiCol_PlotLines);
            const auto thickness = selected ? 3.0F : (related ? 2.2F : 1.4F);
            draw_dashed_line(draw_list, start, finish, color, thickness,
                             std::max(10.0F * transform.zoom, 4.0F),
                             std::max(6.0F * transform.zoom, 3.0F));
            draw_arrow(draw_list, start, finish, color, std::max(transform.zoom, 0.65F));
        } else {
            const auto related = assignment_related_to_selected_task(edge, selection);
            const auto color =
                ImGui::GetColorU32(related ? ImGuiCol_HeaderHovered : ImGuiCol_TextDisabled);
            draw_dashed_line(draw_list, start, finish, color, related ? 2.2F : 1.3F,
                             std::max(2.0F * transform.zoom, 2.0F),
                             std::max(6.0F * transform.zoom, 4.0F));
        }
    }
}

void draw_resource_node(ImDrawList* draw_list, const GuiGraphNode& node,
                        const ScreenTransform& transform,
                        const ExperimentPresentationSnapshot& experiment,
                        const GuiSelection& selection, std::optional<ResourceId> drag_target,
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
    draw_list->AddLine({rect.minimum.x, rect.minimum.y + 42.0F * transform.zoom},
                       {rect.maximum.x, rect.minimum.y + 42.0F * transform.zoom}, border, 1.2F);
    draw_list->AddText(
        nullptr, sanitize_gui_font_size(ImGui::GetFontSize() * transform.zoom),
        {rect.minimum.x + 12.0F * transform.zoom, rect.minimum.y + 11.0F * transform.zoom},
        ImGui::GetColorU32(ImGuiCol_Text), node.label.c_str());
}

void draw_task_node(ImDrawList* draw_list, const GuiGraphNode& node,
                    const ScreenTransform& transform,
                    const ExperimentPresentationSnapshot& experiment,
                    const GuiSimulationSession& session, const GuiSelection& selection,
                    bool dragging, bool read_only_preview) {
    const auto rect = node_rect(node, transform);
    const auto task_id = std::get<TaskId>(node.entity);
    const auto selected = selection.task_id() == task_id;
    const auto draft_resource =
        read_only_preview ? std::optional<ResourceId>{} : session.draft().assignment(task_id);
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
    draw_list->AddText(
        nullptr, sanitize_gui_font_size(ImGui::GetFontSize() * transform.zoom),
        {rect.minimum.x + 10.0F * transform.zoom, rect.minimum.y + 8.0F * transform.zoom},
        ImGui::GetColorU32(ImGuiCol_Text), node.label.c_str());
    if (draft_changed && draft_resource.has_value()) {
        const auto draft_label = "Draft R" + std::to_string(draft_resource->value());
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
               ImVec2 canvas_size) {
    constexpr float padding = 32.0F;
    const auto available_width = std::max(canvas_size.x - 2.0F * padding, 1.0F);
    const auto available_height = std::max(canvas_size.y - 2.0F * padding, 1.0F);
    state.zoom = std::clamp(std::min(available_width / graph.logical_size.width,
                                     available_height / graph.logical_size.height),
                            minimum_zoom, maximum_zoom);
    state.pan_x = (canvas_size.x - graph.logical_size.width * state.zoom) * 0.5F;
    state.pan_y = (canvas_size.y - graph.logical_size.height * state.zoom) * 0.5F;
    state.fit_requested = false;
}

void update_zoom(ArchitectureViewState& state, ImVec2 canvas_origin, ImVec2 mouse, float wheel) {
    const auto old_zoom = state.zoom;
    const auto factor = wheel > 0.0F ? 1.12F : (1.0F / 1.12F);
    state.zoom = std::clamp(old_zoom * factor, minimum_zoom, maximum_zoom);
    const auto logical_x = (mouse.x - canvas_origin.x - state.pan_x) / old_zoom;
    const auto logical_y = (mouse.y - canvas_origin.y - state.pan_y) / old_zoom;
    state.pan_x = mouse.x - canvas_origin.x - logical_x * state.zoom;
    state.pan_y = mouse.y - canvas_origin.y - logical_y * state.zoom;
}

void reject_drop(ArchitectureViewState& state, TaskId task_id, std::string reason) {
    state.status = "Cannot assign T" + std::to_string(task_id.value()) + ": " + std::move(reason);
    state.status_error = true;
}

} // namespace

bool draw_architecture_view(const GuiArchitectureGraph& graph, GuiSimulationSession& session,
                            const ExperimentPresentationSnapshot& experiment,
                            GuiSelection& selection, ArchitectureViewState& state,
                            bool read_only_preview) {
    if (read_only_preview) {
        state.pressed_task.reset();
        state.dragging_task = false;
    }
    if (ImGui::Button("Fit to view")) {
        state.fit_requested = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(read_only_preview
                            ? "Zoom %.0f%% | wheel: zoom | middle-drag: pan | read-only preview"
                            : "Zoom %.0f%% | wheel: zoom | middle-drag: pan | drag task: draft",
                        state.zoom * 100.0F);
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
        fit_graph(state, graph, canvas_size);
    }
    if (canvas_hovered && ImGui::GetIO().MouseWheel != 0.0F) {
        update_zoom(state, canvas_origin, mouse, ImGui::GetIO().MouseWheel);
    }
    if (canvas_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
        state.pan_x += ImGui::GetIO().MouseDelta.x;
        state.pan_y += ImGui::GetIO().MouseDelta.y;
    }

    const ScreenTransform transform{
        .origin = canvas_origin, .pan_x = state.pan_x, .pan_y = state.pan_y, .zoom = state.zoom};
    bool open_inspector = false;
    if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto* task = hit_node(graph, transform, mouse, GuiGraphNodeKind::Task);
        const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
        const auto* route = hit_route(graph, transform, mouse);
        if (task != nullptr) {
            select_graph_node(selection, *task);
            if (!read_only_preview) {
                state.pressed_task = std::get<TaskId>(task->entity);
            }
            open_inspector = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        } else if (route != nullptr) {
            select_graph_edge(selection, *route);
            open_inspector = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        } else if (resource != nullptr) {
            select_graph_node(selection, *resource);
            open_inspector = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        }
    }

    if (!read_only_preview && state.pressed_task.has_value() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        state.dragging_task = true;
    }
    if (state.pressed_task.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const auto task_id = *state.pressed_task;
        if (state.dragging_task) {
            const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
            if (resource == nullptr) {
                reject_drop(state, task_id, "drop onto a resource container");
            } else {
                const auto resource_id = std::get<ResourceId>(resource->entity);
                if (!session.draft_editable()) {
                    reject_drop(state, task_id,
                                "the run is Running; pause it before editing the draft");
                } else if (!graph_assignment_accessible(experiment, task_id, resource_id)) {
                    reject_drop(state, task_id,
                                "resource R" + std::to_string(resource_id.value()) +
                                    " has no configured execution profile for this task");
                } else if (!session.set_draft_assignment(task_id, resource_id)) {
                    reject_drop(state, task_id,
                                "resource R" + std::to_string(resource_id.value()) +
                                    " could not be written to the pending draft");
                } else {
                    state.status = "Draft assignment updated: T" + std::to_string(task_id.value()) +
                                   " -> R" + std::to_string(resource_id.value()) +
                                   ". Use Apply and reset to activate it.";
                    state.status_error = false;
                }
            }
        }
        state.pressed_task.reset();
        state.dragging_task = false;
    }

    std::optional<ResourceId> drag_target;
    bool valid_drag_target = false;
    if (state.dragging_task) {
        const auto* resource = hit_node(graph, transform, mouse, GuiGraphNodeKind::Resource);
        if (resource != nullptr) {
            drag_target = std::get<ResourceId>(resource->entity);
            valid_drag_target =
                session.draft_editable() &&
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
        draw_task_node(draw_list, node, transform, experiment, session, selection, dragging,
                       read_only_preview);
    }
    if (state.dragging_task) {
        draw_list->AddLine(
            mouse, {mouse.x + 20.0F, mouse.y + 20.0F},
            ImGui::GetColorU32(valid_drag_target ? ImGuiCol_CheckMark : ImGuiCol_PlotHistogram),
            3.0F);
    }
    draw_list->PopClipRect();
    return open_inspector;
}

} // namespace cpssim::gui
