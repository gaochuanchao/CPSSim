/***
 * File: src/cpssim/gui/architecture_graph.cpp
 * Purpose: Build the complete deterministic G04 graph and logical layout.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/architecture_graph.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace cpssim {
namespace {

constexpr float graph_margin = 24.0F;
constexpr float resource_header_height = 30.0F;
constexpr float resource_bottom_padding = 12.0F;
constexpr float resource_gap = 30.0F;
constexpr float task_width = 168.0F;
constexpr float task_height = 26.0F;
constexpr float task_horizontal_gap = 92.0F;
constexpr float task_vertical_gap = 12.0F;
constexpr float task_left_padding = 42.0F;

std::size_t task_index(const std::vector<GuiTaskPresentation>& tasks, TaskId task_id) {
    const auto found =
        std::lower_bound(tasks.begin(), tasks.end(), task_id,
                         [](const GuiTaskPresentation& task, TaskId id) { return task.id < id; });
    return found != tasks.end() && found->id == task_id
               ? static_cast<std::size_t>(std::distance(tasks.begin(), found))
               : tasks.size();
}

std::vector<std::size_t> build_task_layers(const std::vector<GuiTaskPresentation>& tasks,
                                           const std::vector<GuiMessageRoutePresentation>& routes) {
    std::vector<std::vector<std::size_t>> successors(tasks.size());
    std::vector<std::size_t> indegree(tasks.size(), 0);
    for (const auto& route : routes) {
        const auto source = task_index(tasks, route.identity.source_task_id);
        const auto destination = task_index(tasks, route.identity.destination_task_id);
        if (source == tasks.size()) {
            throw std::invalid_argument{
                "architecture graph route T" +
                std::to_string(route.identity.source_task_id.value()) + " -> T" +
                std::to_string(route.identity.destination_task_id.value()) + ": source task T" +
                std::to_string(route.identity.source_task_id.value()) + " is unavailable"};
        }
        if (destination == tasks.size()) {
            throw std::invalid_argument{
                "architecture graph route T" +
                std::to_string(route.identity.source_task_id.value()) + " -> T" +
                std::to_string(route.identity.destination_task_id.value()) +
                ": destination task T" +
                std::to_string(route.identity.destination_task_id.value()) + " is unavailable"};
        }
        successors[source].push_back(destination);
        ++indegree[destination];
    }
    for (auto& task_successors : successors) {
        std::sort(task_successors.begin(), task_successors.end());
    }

    std::vector<std::size_t> ready;
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        if (indegree[index] == 0) {
            ready.push_back(index);
        }
    }

    std::vector<std::size_t> layers(tasks.size(), 0);
    std::size_t processed = 0;
    while (!ready.empty()) {
        const auto source = ready.front();
        ready.erase(ready.begin());
        ++processed;
        for (const auto destination : successors[source]) {
            layers[destination] = std::max(layers[destination], layers[source] + 1);
            --indegree[destination];
            if (indegree[destination] == 0) {
                const auto insertion = std::lower_bound(ready.begin(), ready.end(), destination);
                ready.insert(insertion, destination);
            }
        }
    }

    if (processed != tasks.size()) {
        for (std::size_t index = 0; index < tasks.size(); ++index) {
            layers[index] = index;
        }
    }
    return layers;
}

std::optional<ResourceId>
assigned_resource(const std::vector<GuiTaskAssignmentPresentation>& assignments, TaskId task_id) {
    const auto found = std::lower_bound(assignments.begin(), assignments.end(), task_id,
                                        [](const GuiTaskAssignmentPresentation& assignment,
                                           TaskId id) { return assignment.task_id < id; });
    return found != assignments.end() && found->task_id == task_id
               ? std::optional<ResourceId>{found->resource_id}
               : std::nullopt;
}

std::size_t maximum_layer(const std::vector<std::size_t>& layers) {
    return layers.empty() ? 0 : *std::max_element(layers.begin(), layers.end());
}

float group_height(const std::vector<GuiTaskPresentation>& tasks,
                   const std::vector<GuiTaskAssignmentPresentation>& assignments,
                   const std::vector<std::size_t>& layers, std::optional<ResourceId> resource_id) {
    std::vector<std::size_t> counts(maximum_layer(layers) + 1, 0);
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        if (assigned_resource(assignments, tasks[index].id) == resource_id) {
            ++counts[layers[index]];
        }
    }
    const auto largest_stack = counts.empty() ? 0 : *std::max_element(counts.begin(), counts.end());
    const auto task_area =
        static_cast<float>(largest_stack) * task_height +
        static_cast<float>(largest_stack > 0 ? largest_stack - 1 : 0) * task_vertical_gap;
    return std::max(142.0F, resource_header_height + task_area + resource_bottom_padding);
}

std::string task_label(const GuiTaskPresentation& task) {
    return task.name + " (T" + std::to_string(task.id.value()) + ')';
}

float compact_task_width(const std::string& label) {
    return std::clamp(16.0F + static_cast<float>(label.size()) * 8.0F, 96.0F, 220.0F);
}

std::string resource_label(const GuiResourcePresentation& resource) {
    return resource.name + " (R" + std::to_string(resource.id.value()) + ')';
}

} // namespace

GuiGraphNodeId task_graph_node_id(TaskId task_id) {
    return {.kind = GuiGraphNodeKind::Task, .entity_value = task_id.value()};
}

GuiGraphNodeId resource_graph_node_id(ResourceId resource_id) {
    return {.kind = GuiGraphNodeKind::Resource, .entity_value = resource_id.value()};
}

GuiArchitectureGraph
build_architecture_graph(const ExperimentPresentationSnapshot& experiment,
                         const std::vector<GuiFunctionalDependency>& functional_dependencies,
                         bool bosch_latency_presentation, const GuiArchitectureWorkspace* layout) {
    auto resources = experiment.resources;
    auto tasks = experiment.tasks;
    auto routes = experiment.routes;
    auto assignments = experiment.assignments;
    std::sort(resources.begin(), resources.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(tasks.begin(), tasks.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(routes.begin(), routes.end(),
              [](const auto& left, const auto& right) { return left.identity < right.identity; });
    std::sort(assignments.begin(), assignments.end(),
              [](const auto& left, const auto& right) { return left.task_id < right.task_id; });

    const auto layers = build_task_layers(tasks, routes);
    const auto layer_count = maximum_layer(layers) + 1;
    const auto resource_width =
        task_left_padding * 2.0F + static_cast<float>(layer_count) * task_width +
        static_cast<float>(layer_count > 0 ? layer_count - 1 : 0) * task_horizontal_gap;

    GuiArchitectureGraph result;
    result.nodes.reserve(resources.size() + tasks.size());
    float next_y = graph_margin;
    for (const auto& resource : resources) {
        const auto height = group_height(tasks, assignments, layers, resource.id);
        result.nodes.push_back({.id = resource_graph_node_id(resource.id),
                                .kind = GuiGraphNodeKind::Resource,
                                .entity = resource.id,
                                .label = resource_label(resource),
                                .position = {.x = graph_margin, .y = next_y},
                                .size = {.width = resource_width, .height = height}});
        next_y += height + resource_gap;
    }

    const auto unassigned_height = group_height(tasks, assignments, layers, std::nullopt);
    const auto unassigned_y = next_y;
    std::vector<std::vector<std::size_t>> group_slots(resources.size() + 1,
                                                      std::vector<std::size_t>(layer_count, 0));
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        const auto resource_id = assigned_resource(assignments, tasks[index].id);
        std::size_t group = resources.size();
        float group_y = unassigned_y;
        if (resource_id.has_value()) {
            const auto resource = std::lower_bound(resources.begin(), resources.end(), *resource_id,
                                                   [](const GuiResourcePresentation& candidate,
                                                      ResourceId id) { return candidate.id < id; });
            if (resource == resources.end() || resource->id != *resource_id) {
                throw std::invalid_argument{"architecture graph assignment for T" +
                                            std::to_string(tasks[index].id.value()) +
                                            ": resource R" + std::to_string(resource_id->value()) +
                                            " is unavailable"};
            }
            group = static_cast<std::size_t>(std::distance(resources.begin(), resource));
            group_y = result.nodes[group].position.y;
        }
        const auto layer = layers[index];
        const auto slot = group_slots[group][layer]++;
        const auto label = task_label(tasks[index]);
        result.nodes.push_back({
            .id = task_graph_node_id(tasks[index].id),
            .kind = GuiGraphNodeKind::Task,
            .entity = tasks[index].id,
            .label = label,
            .position = {.x = graph_margin + task_left_padding +
                              static_cast<float>(layer) * (task_width + task_horizontal_gap),
                         .y = group_y + resource_header_height +
                              static_cast<float>(slot) * (task_height + task_vertical_gap)},
            .size = {.width = compact_task_width(label), .height = task_height},
        });
    }
    for (auto& resource_node : result.nodes) {
        if (resource_node.kind != GuiGraphNodeKind::Resource)
            continue;
        const auto resource_id = std::get<ResourceId>(resource_node.entity);
        auto right = resource_node.position.x +
                     std::max(48.0F, 24.0F + static_cast<float>(resource_node.label.size()) * 8.0F);
        auto bottom = resource_node.position.y + 48.0F;
        for (const auto& task_node : result.nodes) {
            if (task_node.kind != GuiGraphNodeKind::Task)
                continue;
            const auto assigned =
                assigned_resource(assignments, std::get<TaskId>(task_node.entity));
            if (assigned != resource_id)
                continue;
            right = std::max(right, task_node.position.x + task_node.size.width + 12.0F);
            bottom = std::max(bottom, task_node.position.y + task_node.size.height + 12.0F);
        }
        resource_node.size = {right - resource_node.position.x, bottom - resource_node.position.y};
    }

    if (group_slots.back() != std::vector<std::size_t>(layer_count, 0)) {
        next_y += unassigned_height + resource_gap;
    }
    result.logical_size = {.width = resource_width + 2.0F * graph_margin,
                           .height = std::max(next_y, graph_margin + task_height) + graph_margin};

    if (layout != nullptr) {
        for (auto& node : result.nodes) {
            if (node.kind == GuiGraphNodeKind::Task) {
                if (const auto* override = find_task_layout(*layout, std::get<TaskId>(node.entity));
                    override != nullptr) {
                    node.position = {override->position.x, override->position.y};
                }
            } else if (const auto* override =
                           find_resource_layout(*layout, std::get<ResourceId>(node.entity));
                       override != nullptr) {
                if (override->position.has_value()) {
                    node.position = {override->position->x, override->position->y};
                }
                if (override->size.has_value()) {
                    node.size = {override->size->width, override->size->height};
                }
            }
            result.logical_size.width = std::max(result.logical_size.width,
                                                 node.position.x + node.size.width + graph_margin);
            result.logical_size.height = std::max(
                result.logical_size.height, node.position.y + node.size.height + graph_margin);
        }
    }

    result.edges.reserve(functional_dependencies.size() + routes.size() + assignments.size());
    for (const auto& dependency : functional_dependencies) {
        const auto source = task_graph_node_id(dependency.source_task_id);
        const auto destination = task_graph_node_id(dependency.destination_task_id);
        if (find_graph_node(result, source) == nullptr ||
            find_graph_node(result, destination) == nullptr)
            throw std::invalid_argument{"functional dependency references an unavailable task"};
        result.edges.push_back({.id = {.kind = GuiGraphEdgeKind::FunctionalDependency,
                                       .source = source,
                                       .destination = destination},
                                .kind = GuiGraphEdgeKind::FunctionalDependency,
                                .source = source,
                                .destination = destination,
                                .route_reference = std::nullopt,
                                .functional_reference = dependency,
                                .assignment_reference = std::nullopt,
                                .connection = GuiConnectionPresentation{
                                    .id = {GuiConnectionKind::Logical, dependency.source_task_id,
                                           dependency.destination_task_id},
                                    .label = dependency.label,
                                    .displayed_latency = 0,
                                    .creates_network_events = false,
                                    .protected_semantics = bosch_latency_presentation}});
    }
    for (const auto& route : routes) {
        const auto source = task_graph_node_id(route.identity.source_task_id);
        const auto destination = task_graph_node_id(route.identity.destination_task_id);
        if (find_graph_node(result, source) == nullptr) {
            throw std::invalid_argument{
                "architecture graph route T" +
                std::to_string(route.identity.source_task_id.value()) + " -> T" +
                std::to_string(route.identity.destination_task_id.value()) + ": source task T" +
                std::to_string(route.identity.source_task_id.value()) + " is unavailable"};
        }
        if (find_graph_node(result, destination) == nullptr) {
            throw std::invalid_argument{
                "architecture graph route T" +
                std::to_string(route.identity.source_task_id.value()) + " -> T" +
                std::to_string(route.identity.destination_task_id.value()) +
                ": destination task T" +
                std::to_string(route.identity.destination_task_id.value()) + " is unavailable"};
        }
        const auto is_comm = route.kind == 0; // 0=Communication, 1=Logical
        result.edges.push_back(
            {.id = {.kind = GuiGraphEdgeKind::MessageRoute,
                    .source = source,
                    .destination = destination},
             .kind = GuiGraphEdgeKind::MessageRoute,
             .source = source,
             .destination = destination,
             .route_reference = route.identity,
             .functional_reference = std::nullopt,
             .assignment_reference = std::nullopt,
             .connection = GuiConnectionPresentation{
                 .id = {is_comm ? GuiConnectionKind::Communication
                                : GuiConnectionKind::Logical,
                        route.identity.source_task_id,
                        route.identity.destination_task_id},
                 .label = is_comm ? "Communication" : "Logical",
                 .displayed_latency = bosch_latency_presentation
                                          ? Tick{80}
                                          : is_comm ? route.delay : Tick{0},
                 .creates_network_events = is_comm,
                 .protected_semantics = bosch_latency_presentation}});
    }
    for (const auto& assignment : assignments) {
        const auto source = task_graph_node_id(assignment.task_id);
        const auto destination = resource_graph_node_id(assignment.resource_id);
        if (find_graph_node(result, source) == nullptr) {
            throw std::invalid_argument{"architecture graph assignment for T" +
                                        std::to_string(assignment.task_id.value()) +
                                        ": task is unavailable"};
        }
        if (find_graph_node(result, destination) == nullptr) {
            throw std::invalid_argument{
                "architecture graph assignment for T" + std::to_string(assignment.task_id.value()) +
                ": resource R" + std::to_string(assignment.resource_id.value()) +
                " is unavailable"};
        }
        result.edges.push_back({.id = {.kind = GuiGraphEdgeKind::Assignment,
                                       .source = source,
                                       .destination = destination},
                                .kind = GuiGraphEdgeKind::Assignment,
                                .source = source,
                                .destination = destination,
                                .route_reference = std::nullopt,
                                .functional_reference = std::nullopt,
                                .assignment_reference = assignment,
                                .connection = std::nullopt});
    }
    return result;
}

const GuiGraphNode* find_graph_node(const GuiArchitectureGraph& graph, GuiGraphNodeId node_id) {
    const auto found =
        std::find_if(graph.nodes.begin(), graph.nodes.end(),
                     [node_id](const GuiGraphNode& node) { return node.id == node_id; });
    return found != graph.nodes.end() ? &*found : nullptr;
}

bool graph_assignment_accessible(const ExperimentPresentationSnapshot& experiment, TaskId task_id,
                                 ResourceId resource_id) {
    return find_task(experiment, task_id) != nullptr &&
           find_resource(experiment, resource_id) != nullptr &&
           std::any_of(experiment.profiles.begin(), experiment.profiles.end(),
                       [task_id, resource_id](const auto& profile) {
                           return profile.task_id == task_id && profile.resource_id == resource_id;
                       });
}

void select_graph_node(StructuralSelection& selection, const GuiGraphNode& node) {
    switch (node.kind) {
    case GuiGraphNodeKind::Task:
        selection.select_task(std::get<TaskId>(node.entity));
        return;
    case GuiGraphNodeKind::Resource:
        selection.select_resource(std::get<ResourceId>(node.entity));
        return;
    }
}

void select_graph_edge(StructuralSelection& selection, const GuiGraphEdge& edge) {
    if (edge.connection.has_value()) {
        selection.select_connection(edge.connection->id);
        return;
    }
    if (edge.kind == GuiGraphEdgeKind::Assignment) {
        if (!edge.assignment_reference.has_value()) {
            throw std::logic_error{"assignment graph edge has no assignment reference"};
        }
        selection.select_task(edge.assignment_reference.value().task_id);
        return;
    }
    throw std::logic_error{"connection graph edge has no connection presentation"};
}

} // namespace cpssim
