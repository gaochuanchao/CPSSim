/***
 * File: apps/gui/views/experiment_explorer.cpp
 * Purpose: Render a stable-ID read-only tree of validated experiment data.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "experiment_explorer.hpp"

#include "imgui.h"

#include <string>

namespace cpssim::gui {
namespace {

/*** Builds a stable immediate-mode ID without using a visible list position. ***/
std::string entity_id(const char* domain, std::uint64_t value) {
    return std::string{domain} + ':' + std::to_string(value);
}

/*** Draws one stable resource leaf and handles resource selection. ***/
void draw_resource(const GuiResourcePresentation& resource, GuiSelection& selection) {
    const auto stable_id = entity_id("resource", resource.id.value());
    ImGui::PushID(stable_id.c_str());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.resource_id() == resource.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const auto label = resource.name + " (R" + std::to_string(resource.id.value()) + ')';
    ImGui::TreeNodeEx("resource", flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_resource(resource.id);
    }
    ImGui::PopID();
}

/*** Draws profiles and applied placement below one task node. ***/
void draw_task_details(const ExperimentPresentationSnapshot& experiment,
                       const GuiTaskPresentation& task) {
    ImGui::TreeNodeEx("Timing", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
    ImGui::Indent();
    ImGui::Text("Period: %lld", static_cast<long long>(task.period));
    ImGui::Text("Deadline: %lld", static_cast<long long>(task.deadline));
    ImGui::Text("Offset: %lld", static_cast<long long>(task.offset));
    ImGui::Unindent();

    if (ImGui::TreeNodeEx("Accessible resources / profiles", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& profile : experiment.profiles) {
            if (profile.task_id != task.id) {
                continue;
            }
            const auto* resource = find_resource(experiment, profile.resource_id);
            const auto resource_name =
                resource != nullptr ? resource->name : std::string{"Unavailable"};
            ImGui::BulletText("%s (R%llu): %lld ticks", resource_name.c_str(),
                              static_cast<unsigned long long>(profile.resource_id.value()),
                              static_cast<long long>(profile.execution_time));
        }
        ImGui::TreePop();
    }

    const auto* assignment = find_assignment(experiment, task.id);
    if (assignment == nullptr) {
        ImGui::TreeNodeEx("Applied assignment: None",
                          ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        return;
    }
    const auto* resource = find_resource(experiment, assignment->resource_id);
    const auto resource_name = resource != nullptr ? resource->name : std::string{"Unavailable"};
    ImGui::TreeNodeEx("assignment", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen,
                      "Applied assignment: %s (R%llu)", resource_name.c_str(),
                      static_cast<unsigned long long>(assignment->resource_id.value()));
}

/*** Draws one task subtree and handles stable task selection. ***/
void draw_task(const ExperimentPresentationSnapshot& experiment, const GuiTaskPresentation& task,
               GuiSelection& selection) {
    const auto stable_id = entity_id("task", task.id.value());
    ImGui::PushID(stable_id.c_str());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.task_id() == task.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const auto label = task.name + " (T" + std::to_string(task.id.value()) + ')';
    const auto open = ImGui::TreeNodeEx("task", flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_task(task.id);
    }
    if (open) {
        ImGui::Text("Priority: %d", task.priority);
        draw_task_details(experiment, task);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

/*** Draws one uniquely identified configured message route. ***/
void draw_route(const GuiMessageRoutePresentation& route, GuiSelection& selection) {
    const auto stable_id = entity_id("route-source", route.identity.source_task_id.value()) + ':' +
                           std::to_string(route.identity.destination_task_id.value());
    ImGui::PushID(stable_id.c_str());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.route_id() == route.identity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TreeNodeEx("route", flags, "T%llu -> T%llu",
                      static_cast<unsigned long long>(route.identity.source_task_id.value()),
                      static_cast<unsigned long long>(route.identity.destination_task_id.value()));
    if (ImGui::IsItemClicked()) {
        selection.select_route(route.identity);
    }
    ImGui::PopID();
}

} // namespace

/*** Draws scheduling, resources, tasks/profiles/placement, and all routes. ***/
void draw_experiment_explorer(const ExperimentPresentationSnapshot& experiment,
                              GuiSelection& selection) {
    ImGuiTreeNodeFlags root_flags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.kind() == GuiSelectionKind::Experiment) {
        root_flags |= ImGuiTreeNodeFlags_Selected;
    }
    const auto root_open = ImGui::TreeNodeEx("Experiment", root_flags);
    if (ImGui::IsItemClicked()) {
        selection.select_experiment();
    }
    if (!root_open) {
        return;
    }

    ImGui::TreeNodeEx("Scheduling", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);

    if (ImGui::TreeNodeEx("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& resource : experiment.resources) {
            draw_resource(resource, selection);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Tasks", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& task : experiment.tasks) {
            draw_task(experiment, task, selection);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Message routes", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (experiment.routes.empty()) {
            ImGui::TextDisabled("None configured");
        }
        for (const auto& route : experiment.routes) {
            draw_route(route, selection);
        }
        ImGui::TreePop();
    }

    ImGui::TreePop();
}

} // namespace cpssim::gui
