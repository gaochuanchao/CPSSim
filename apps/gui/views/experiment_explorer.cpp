/*** Render the system draft as an Explorer-owned structural object manager. ***/

#include "experiment_explorer.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

std::string resource_label(const DraftResource& resource) {
    return resource.name + " (R" + std::to_string(resource.id.value()) + ')';
}

std::string task_label(const DraftTask& task) {
    return task.name + " (T" + std::to_string(task.id.value()) + ')';
}

std::string profile_label(const EditableSystemDraft& draft, const DraftExecutionProfile& profile) {
    const auto task =
        std::find_if(draft.tasks().begin(), draft.tasks().end(),
                     [&profile](const auto& row) { return row.id == profile.task_id; });
    const auto resource =
        std::find_if(draft.resources().begin(), draft.resources().end(),
                     [&profile](const auto& row) { return row.id == profile.resource_id; });
    const auto task_name = task == draft.tasks().end() ? "Unknown task" : task->name;
    const auto resource_name =
        resource == draft.resources().end() ? "Unknown resource" : resource->name;
    return task_name + " -> " + resource_name;
}

std::string route_label(const EditableSystemDraft& draft, const DraftMessageRoute& route) {
    const auto task_name = [&draft](TaskId id) {
        const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                        [id](const auto& row) { return row.id == id; });
        return found == draft.tasks().end() ? std::string{"Unknown"} : found->name;
    };
    return task_name(route.source_task_id) + " -> " + task_name(route.destination_task_id);
}

void apply_action(const SystemExplorerActionResult& result, ExperimentExplorerViewState& state) {
    if (!result.diagnostic.empty()) {
        state.status = result.diagnostic;
        state.status_error = true;
        return;
    }
    state.expand_section = result.expand_section;
    state.scroll_to = result.scroll_to;
    state.focus_request = result.focus;
    state.status = result.changed ? "Draft structure updated." : std::string{};
    state.status_error = false;
}

bool is_selected(const StructuralSelection& selection, StructuralSection section) {
    return selection.section() == section;
}

void maybe_scroll(const StructuralSelection& current, ExperimentExplorerViewState& state) {
    if (state.scroll_to == current) {
        ImGui::SetScrollHereY(0.5F);
        state.scroll_to.reset();
    }
}

void draw_disabled_explanation(bool editing_enabled) {
    if (!editing_enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Pause or reset the active run before editing structure.");
    }
}

void section_create_menu(const char* action_label, StructuralSection section,
                         EditableSystemDraft& draft, StructuralSelection& selection,
                         SystemExplorerInteraction& interaction, bool editing_enabled,
                         ExperimentExplorerViewState& state) {
    const auto availability = interaction.create_availability(section, draft);
    ImGui::BeginDisabled(!editing_enabled || !availability.available);
    if (ImGui::MenuItem(action_label)) {
        apply_action(interaction.create(section, draft, selection), state);
    }
    draw_disabled_explanation(editing_enabled);
    ImGui::EndDisabled();
    if (!availability.available) {
        ImGui::TextDisabled("%s", availability.explanation.c_str());
    }
}

void entity_menu(const char* duplicate_label, const char* delete_label,
                 const StructuralSelection& target, EditableSystemDraft& draft,
                 std::vector<DraftTaskAssignment>& assignments, StructuralSelection& selection,
                 SystemExplorerInteraction& interaction, bool editing_enabled,
                 ExperimentExplorerViewState& state) {
    ImGui::BeginDisabled(!editing_enabled);
    if (ImGui::MenuItem(duplicate_label)) {
        apply_action(interaction.duplicate(target, draft, selection), state);
    }
    if (ImGui::MenuItem(delete_label)) {
        static_cast<void>(interaction.request_delete(target, draft, assignments));
        ImGui::OpenPopup("Confirm structural deletion");
    }
    draw_disabled_explanation(editing_enabled);
    ImGui::EndDisabled();
}

void draw_resource_node(const DraftResource& resource, EditableSystemDraft& draft,
                        std::vector<DraftTaskAssignment>& assignments,
                        StructuralSelection& selection, SystemExplorerInteraction& interaction,
                        bool editing_enabled, ExperimentExplorerViewState& state) {
    ImGui::PushID(static_cast<int>(resource.id.value()));
    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                 ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.resource_id() == resource.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TreeNodeEx("resource", flags, "%s", resource_label(resource).c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_resource(resource.id);
    }
    StructuralSelection target;
    target.select_resource(resource.id);
    maybe_scroll(target, state);
    if (ImGui::BeginPopupContextItem("resource actions")) {
        entity_menu("Duplicate Resource", "Delete Resource...", target, draft, assignments,
                    selection, interaction, editing_enabled, state);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void draw_task_node(const DraftTask& task, EditableSystemDraft& draft,
                    std::vector<DraftTaskAssignment>& assignments, StructuralSelection& selection,
                    SystemExplorerInteraction& interaction, bool editing_enabled,
                    ExperimentExplorerViewState& state) {
    ImGui::PushID(static_cast<int>(task.id.value()));
    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                 ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.task_id() == task.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TreeNodeEx("task", flags, "%s", task_label(task).c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_task(task.id);
    }
    StructuralSelection target;
    target.select_task(task.id);
    maybe_scroll(target, state);
    if (ImGui::BeginPopupContextItem("task actions")) {
        entity_menu("Duplicate Task", "Delete Task...", target, draft, assignments, selection,
                    interaction, editing_enabled, state);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void draw_profile_node(const DraftExecutionProfile& profile, EditableSystemDraft& draft,
                       std::vector<DraftTaskAssignment>& assignments,
                       StructuralSelection& selection, SystemExplorerInteraction& interaction,
                       bool editing_enabled, ExperimentExplorerViewState& state) {
    const DraftExecutionProfileKey key{profile.task_id, profile.resource_id};
    ImGui::PushID(static_cast<int>(profile.task_id.value()));
    ImGui::PushID(static_cast<int>(profile.resource_id.value()));
    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                 ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.execution_profile() == key) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TreeNodeEx("profile", flags, "%s", profile_label(draft, profile).c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_execution_profile(key);
    }
    StructuralSelection target;
    target.select_execution_profile(key);
    maybe_scroll(target, state);
    if (ImGui::BeginPopupContextItem("profile actions")) {
        entity_menu("Duplicate Profile", "Delete Profile...", target, draft, assignments, selection,
                    interaction, editing_enabled, state);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

void draw_route_node(const DraftMessageRoute& route, EditableSystemDraft& draft,
                     std::vector<DraftTaskAssignment>& assignments, StructuralSelection& selection,
                     SystemExplorerInteraction& interaction, bool editing_enabled,
                     ExperimentExplorerViewState& state) {
    const DraftMessageRouteKey key{route.source_task_id, route.destination_task_id};
    ImGui::PushID(static_cast<int>(route.source_task_id.value()));
    ImGui::PushID(static_cast<int>(route.destination_task_id.value()));
    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                 ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.message_route() == key) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TreeNodeEx("route", flags, "%s", route_label(draft, route).c_str());
    if (ImGui::IsItemClicked()) {
        selection.select_message_route(key);
    }
    StructuralSelection target;
    target.select_message_route(key);
    maybe_scroll(target, state);
    if (ImGui::BeginPopupContextItem("route actions")) {
        entity_menu("Duplicate Route", "Delete Route...", target, draft, assignments, selection,
                    interaction, editing_enabled, state);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

template <typename DrawRows>
void draw_section(const char* label, StructuralSection section, const char* create_label,
                  EditableSystemDraft& draft, StructuralSelection& selection,
                  SystemExplorerInteraction& interaction, bool editing_enabled,
                  ExperimentExplorerViewState& state, DrawRows draw_rows) {
    if (state.expand_section == section) {
        ImGui::SetNextItemOpen(true);
        state.expand_section.reset();
    }
    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (is_selected(selection, section)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const auto open = ImGui::TreeNodeEx(label, flags);
    if (ImGui::IsItemClicked()) {
        selection.select_section(section);
    }
    if (ImGui::BeginPopupContextItem()) {
        section_create_menu(create_label, section, draft, selection, interaction, editing_enabled,
                            state);
        ImGui::EndPopup();
    }
    if (open) {
        draw_rows();
        ImGui::TreePop();
    }
}

void draw_delete_confirmation(EditableSystemDraft& draft,
                              std::vector<DraftTaskAssignment>& assignments,
                              StructuralSelection& selection,
                              SystemExplorerInteraction& interaction,
                              ExperimentExplorerViewState& state) {
    if (!ImGui::BeginPopupModal("Confirm structural deletion", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const auto& pending = interaction.pending_delete();
    if (!pending.has_value()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextWrapped("Delete the selected structural entity from the draft?");
    ImGui::SeparatorText("This also removes");
    ImGui::BulletText("%zu execution profile(s)", pending->structural.execution_profiles);
    ImGui::BulletText("%zu incoming route(s)", pending->structural.incoming_routes);
    ImGui::BulletText("%zu outgoing route(s)", pending->structural.outgoing_routes);
    ImGui::BulletText("%zu draft run-plan assignment(s)", pending->run_plan_assignments);
    ImGui::TextDisabled("The applied system is unchanged until Apply and restart.");
    if (ImGui::Button("Cancel")) {
        interaction.cancel_delete();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete from draft")) {
        apply_action(interaction.confirm_delete(draft, assignments, selection), state);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void draw_read_only_applied(const ExperimentPresentationSnapshot& applied,
                            StructuralSelection& selection) {
    ImGui::TextDisabled("Structural editing is unavailable for this adapter-owned project.");
    if (ImGui::TreeNodeEx("System", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNodeEx("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& resource : applied.resources) {
                ImGui::BulletText("%s (R%llu)", resource.name.c_str(),
                                  static_cast<unsigned long long>(resource.id.value()));
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Tasks", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& task : applied.tasks) {
                ImGui::BulletText("%s (T%llu)", task.name.c_str(),
                                  static_cast<unsigned long long>(task.id.value()));
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    selection.select_system();
}

} // namespace

void draw_experiment_explorer(const ExperimentPresentationSnapshot& applied,
                              EditableSystemDraft* draft,
                              std::vector<DraftTaskAssignment>& assignments,
                              StructuralSelection& selection,
                              SystemExplorerInteraction& interaction, bool editing_enabled,
                              ExperimentExplorerViewState& state) {
    if (draft == nullptr) {
        draw_read_only_applied(applied, selection);
        return;
    }
    synchronize_structural_selection(selection, *draft);
    auto root_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.kind() == StructuralSelectionKind::System) {
        root_flags |= ImGuiTreeNodeFlags_Selected;
    }
    const auto root_open = ImGui::TreeNodeEx("System", root_flags);
    if (ImGui::IsItemClicked()) {
        selection.select_system();
    }
    if (root_open) {
        draw_section("Resources", StructuralSection::Resources, "Add Resource", *draft, selection,
                     interaction, editing_enabled, state, [&] {
                         const auto rows = draft->resources();
                         for (const auto& resource : rows) {
                             draw_resource_node(resource, *draft, assignments, selection,
                                                interaction, editing_enabled, state);
                         }
                     });
        draw_section("Tasks", StructuralSection::Tasks, "Add Task", *draft, selection, interaction,
                     editing_enabled, state, [&] {
                         const auto rows = draft->tasks();
                         for (const auto& task : rows) {
                             draw_task_node(task, *draft, assignments, selection, interaction,
                                            editing_enabled, state);
                         }
                     });
        draw_section("Execution Profiles", StructuralSection::ExecutionProfiles,
                     "Add Execution Profile", *draft, selection, interaction, editing_enabled,
                     state, [&] {
                         const auto rows = draft->profiles();
                         for (const auto& profile : rows) {
                             draw_profile_node(profile, *draft, assignments, selection, interaction,
                                               editing_enabled, state);
                         }
                     });
        draw_section("Message Routes", StructuralSection::MessageRoutes, "Add Message Route",
                     *draft, selection, interaction, editing_enabled, state, [&] {
                         const auto rows = draft->routes();
                         for (const auto& route : rows) {
                             draw_route_node(route, *draft, assignments, selection, interaction,
                                             editing_enabled, state);
                         }
                     });
        ImGui::TreePop();
    }
    if (!state.status.empty()) {
        const auto color = state.status_error ? ImVec4{1.0F, 0.48F, 0.32F, 1.0F}
                                              : ImVec4{0.45F, 0.85F, 0.55F, 1.0F};
        ImGui::TextColored(color, "%s", state.status.c_str());
    }
    draw_delete_confirmation(*draft, assignments, selection, interaction, state);
}

} // namespace cpssim::gui
