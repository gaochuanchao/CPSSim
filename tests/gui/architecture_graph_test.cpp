/***
 * File: tests/gui/architecture_graph_test.cpp
 * Purpose: Verify G04 graph completeness, deterministic layout, and selection.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/architecture_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace cpssim;

ExperimentConfig make_graph_config(bool reverse = false, bool cyclic = false,
                                   std::string first_name = "sensor") {
    std::vector<ResourceSpec> resources;
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    std::vector<MessageRouteSpec> routes;

    const auto first_route = MessageRouteSpec{.source_task_id = TaskId{1},
                                              .destination_task_id = TaskId{2},
                                              .send_offset = 1,
                                              .delay = 3};
    const auto return_route = MessageRouteSpec{.source_task_id = TaskId{2},
                                               .destination_task_id = TaskId{1},
                                               .send_offset = 2,
                                               .delay = 4};
    if (reverse) {
        resources.emplace_back(ResourceId{9}, "cloud");
        resources.emplace_back(ResourceId{4}, "local");
        tasks.emplace_back(TaskId{3}, "disconnected",
                           PeriodicTimingSpec{.period = 30, .deadline = 30, .offset = 0}, 3);
        tasks.emplace_back(TaskId{2}, "controller",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2);
        tasks.emplace_back(TaskId{1}, std::move(first_name),
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 1);
        profiles.push_back(
            {.task_id = TaskId{3}, .resource_id = ResourceId{9}, .execution_time = 5});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{9}, .execution_time = 3});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{9}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{4}, .execution_time = 1});
        if (cyclic) {
            routes.push_back(return_route);
        }
        routes.push_back(first_route);
    } else {
        resources.emplace_back(ResourceId{4}, "local");
        resources.emplace_back(ResourceId{9}, "cloud");
        tasks.emplace_back(TaskId{1}, std::move(first_name),
                           PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0}, 1);
        tasks.emplace_back(TaskId{2}, "controller",
                           PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 2);
        tasks.emplace_back(TaskId{3}, "disconnected",
                           PeriodicTimingSpec{.period = 30, .deadline = 30, .offset = 0}, 3);
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{4}, .execution_time = 1});
        profiles.push_back(
            {.task_id = TaskId{1}, .resource_id = ResourceId{9}, .execution_time = 2});
        profiles.push_back(
            {.task_id = TaskId{2}, .resource_id = ResourceId{9}, .execution_time = 3});
        profiles.push_back(
            {.task_id = TaskId{3}, .resource_id = ResourceId{9}, .execution_time = 5});
        routes.push_back(first_route);
        if (cyclic) {
            routes.push_back(return_route);
        }
    }

    return ExperimentConfig{std::chrono::microseconds{100},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            std::move(resources),
                            std::move(tasks),
                            std::move(profiles),
                            std::move(routes)};
}

ExperimentPresentationSnapshot make_graph_presentation(bool reverse = false, bool cyclic = false,
                                                       std::string first_name = "sensor") {
    const auto config = make_graph_config(reverse, cyclic, std::move(first_name));
    std::vector<TaskAssignment> assignments;
    if (reverse) {
        assignments = {{.task_id = TaskId{3}, .resource_id = ResourceId{9}},
                       {.task_id = TaskId{2}, .resource_id = ResourceId{9}},
                       {.task_id = TaskId{1}, .resource_id = ResourceId{4}}};
    } else {
        assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{4}},
                       {.task_id = TaskId{2}, .resource_id = ResourceId{9}},
                       {.task_id = TaskId{3}, .resource_id = ResourceId{9}}};
    }
    return build_experiment_presentation(config, assignments);
}

std::string graph_error(const ExperimentPresentationSnapshot& experiment) {
    try {
        static_cast<void>(build_architecture_graph(experiment));
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    throw std::logic_error{"invalid graph input unexpectedly succeeded"};
}

ExperimentPresentationSnapshot make_large_graph_presentation() {
    std::vector<ResourceSpec> resources;
    std::vector<TaskSpec> tasks;
    std::vector<TaskResourceProfile> profiles;
    std::vector<MessageRouteSpec> routes;
    std::vector<TaskAssignment> assignments;
    for (std::uint64_t resource = 1; resource <= 32; ++resource) {
        resources.emplace_back(ResourceId{resource}, "resource-" + std::to_string(resource));
    }
    for (std::uint64_t task = 1; task <= 100; ++task) {
        const auto resource = ResourceId{1 + ((task - 1) % 32)};
        tasks.emplace_back(TaskId{task}, "task-" + std::to_string(task),
                           PeriodicTimingSpec{.period = 100, .deadline = 100, .offset = 0},
                           static_cast<Priority>(task));
        profiles.push_back({.task_id = TaskId{task}, .resource_id = resource, .execution_time = 1});
        assignments.push_back({.task_id = TaskId{task}, .resource_id = resource});
        if (task < 100) {
            routes.push_back({.source_task_id = TaskId{task},
                              .destination_task_id = TaskId{task + 1},
                              .send_offset = 1,
                              .delay = 1});
        }
    }
    const ExperimentConfig config{std::chrono::microseconds{100},
                                  SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                                  std::move(resources),
                                  std::move(tasks),
                                  std::move(profiles),
                                  std::move(routes)};
    return build_experiment_presentation(config, assignments);
}

TEST_CASE("architecture graph contains every stable entity and relation",
          "[gui][architecture][graph]") {
    const auto graph = build_architecture_graph(make_graph_presentation());

    REQUIRE((graph.nodes.size() == 5));
    REQUIRE((graph.edges.size() == 4));
    REQUIRE((find_graph_node(graph, resource_graph_node_id(ResourceId{4})) != nullptr));
    REQUIRE((find_graph_node(graph, resource_graph_node_id(ResourceId{9})) != nullptr));
    REQUIRE((find_graph_node(graph, task_graph_node_id(TaskId{1})) != nullptr));
    REQUIRE((find_graph_node(graph, task_graph_node_id(TaskId{2})) != nullptr));
    REQUIRE((find_graph_node(graph, task_graph_node_id(TaskId{3})) != nullptr));

    const auto& route = graph.edges.front();
    REQUIRE((route.kind == GuiGraphEdgeKind::MessageRoute));
    REQUIRE((route.source == task_graph_node_id(TaskId{1})));
    REQUIRE((route.destination == task_graph_node_id(TaskId{2})));
    REQUIRE((route.route_reference == GuiRouteIdentity{TaskId{1}, TaskId{2}}));
    REQUIRE(route.connection->id.kind == GuiConnectionKind::Communication);
    REQUIRE(route.connection->displayed_latency == 3);
    REQUIRE_FALSE(route.assignment_reference.has_value());

    const auto& assignment = graph.edges.back();
    REQUIRE((assignment.kind == GuiGraphEdgeKind::Assignment));
    REQUIRE(assignment.assignment_reference.has_value());
    REQUIRE((find_graph_node(graph, assignment.source) != nullptr));
    REQUIRE((find_graph_node(graph, assignment.destination) != nullptr));
}

TEST_CASE("architecture graph layout ignores declarations and labels",
          "[gui][architecture][determinism]") {
    const auto forward = build_architecture_graph(make_graph_presentation());
    const auto repeated = build_architecture_graph(make_graph_presentation());
    const auto reversed = build_architecture_graph(make_graph_presentation(true));
    const auto renamed = build_architecture_graph(make_graph_presentation(false, false, "renamed"));

    REQUIRE((forward == repeated));
    REQUIRE((forward == reversed));
    REQUIRE((renamed.nodes.size() == forward.nodes.size()));
    for (std::size_t index = 0; index < forward.nodes.size(); ++index) {
        REQUIRE((renamed.nodes[index].id == forward.nodes[index].id));
        REQUIRE((renamed.nodes[index].position == forward.nodes[index].position));
    }
    REQUIRE((renamed.edges.size() == forward.edges.size()));
    for (std::size_t index = 0; index < forward.edges.size(); ++index) {
        REQUIRE((renamed.edges[index].id == forward.edges[index].id));
    }
}

TEST_CASE("architecture graph keeps disconnected and cyclic tasks visible",
          "[gui][architecture][cycle]") {
    const auto disconnected = build_architecture_graph(make_graph_presentation());
    const auto* isolated = find_graph_node(disconnected, task_graph_node_id(TaskId{3}));
    REQUIRE((isolated != nullptr));
    REQUIRE((isolated->position.x >= 0.0F));
    REQUIRE((isolated->position.y >= 0.0F));

    const auto cyclic = build_architecture_graph(make_graph_presentation(false, true));
    const auto cyclic_again = build_architecture_graph(make_graph_presentation(false, true));
    REQUIRE((cyclic == cyclic_again));
    REQUIRE((cyclic.nodes.size() == 5));
    REQUIRE((cyclic.edges.size() == 5));
}

TEST_CASE("architecture graph maps entities into shared selection",
          "[gui][architecture][selection]") {
    const auto graph = build_architecture_graph(make_graph_presentation());
    StructuralSelection selection;

    select_graph_node(selection, *find_graph_node(graph, task_graph_node_id(TaskId{1})));
    REQUIRE((selection.task_id() == TaskId{1}));
    select_graph_node(selection, *find_graph_node(graph, resource_graph_node_id(ResourceId{9})));
    REQUIRE((selection.resource_id() == ResourceId{9}));
    select_graph_edge(selection, graph.edges.front());
    REQUIRE((selection.connection() ==
             GuiConnectionId{GuiConnectionKind::Communication, TaskId{1}, TaskId{2}}));
    select_graph_edge(selection, graph.edges.back());
    REQUIRE((selection.task_id() == graph.edges.back().assignment_reference->task_id));
}

TEST_CASE("architecture graph reports inaccessible draft targets and broken endpoints",
          "[gui][architecture][invalid]") {
    const auto experiment = make_graph_presentation();
    REQUIRE(graph_assignment_accessible(experiment, TaskId{1}, ResourceId{4}));
    REQUIRE_FALSE(graph_assignment_accessible(experiment, TaskId{2}, ResourceId{4}));
    REQUIRE_FALSE(graph_assignment_accessible(experiment, TaskId{99}, ResourceId{4}));

    auto broken = experiment;
    broken.routes[0].identity.destination_task_id = TaskId{99};
    REQUIRE((graph_error(broken) ==
             "architecture graph route T1 -> T99: destination task T99 is unavailable"));
}

TEST_CASE("architecture graph covers the initial large-graph interaction target",
          "[gui][architecture][large]") {
    const auto experiment = make_large_graph_presentation();
    const auto graph = build_architecture_graph(experiment);

    REQUIRE((graph.nodes.size() == 132));
    REQUIRE((graph.edges.size() == 199));
    REQUIRE((find_graph_node(graph, task_graph_node_id(TaskId{100})) != nullptr));
    REQUIRE((find_graph_node(graph, resource_graph_node_id(ResourceId{32})) != nullptr));
    REQUIRE((graph.logical_size.width > 0.0F));
    REQUIRE((graph.logical_size.height > 0.0F));
    REQUIRE((graph == build_architecture_graph(experiment)));
}

TEST_CASE("functional dependencies are distinct presentation-only graph edges",
          "[gui][architecture][functional]") {
    const auto experiment = make_graph_presentation();
    const std::vector<GuiFunctionalDependency> dependencies{{TaskId{1}, TaskId{2}, "data"}};
    const auto graph = build_architecture_graph(experiment, dependencies);
    REQUIRE(graph.edges.front().kind == GuiGraphEdgeKind::FunctionalDependency);
    REQUIRE(graph.edges.front().functional_reference == dependencies.front());
    REQUIRE(graph.edges.front().connection->id.kind == GuiConnectionKind::Logical);
    REQUIRE(graph.edges.front().connection->displayed_latency == 0);
    REQUIRE_FALSE(graph.edges.front().connection->creates_network_events);
    REQUIRE(graph.edges.front().route_reference == std::nullopt);
    REQUIRE(experiment.routes.size() == 1);
}

TEST_CASE("Bosch connection presentation hides adapter handoff and reports latency 80",
          "[gui][architecture][connection]") {
    const auto graph = build_architecture_graph(
        make_graph_presentation(), {{TaskId{2}, TaskId{3}, "logical"}}, true);
    const auto communication = std::find_if(graph.edges.begin(), graph.edges.end(), [](const auto& edge) {
        return edge.connection.has_value() &&
               edge.connection->id.kind == GuiConnectionKind::Communication;
    });
    REQUIRE(communication != graph.edges.end());
    REQUIRE(communication->connection->displayed_latency == 80);
    REQUIRE(communication->connection->creates_network_events);
    REQUIRE(communication->route_reference->source_task_id == TaskId{1});
}

} // namespace
