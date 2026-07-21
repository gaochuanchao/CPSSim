/*** Render selection-driven structural properties without mutating active runtime. ***/

#include "system_builder.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

constexpr ImVec4 error_color{1.0F, 0.48F, 0.32F, 1.0F};

template <std::size_t Size> std::array<char, Size> text_buffer(const std::string& value) {
    std::array<char, Size> result{};
    std::copy_n(value.begin(), std::min(value.size(), result.size() - 1), result.begin());
    return result;
}

void draw_field_diagnostics(const SystemDraftBuildResult& validation, SystemDraftEntityKind kind,
                            std::size_t index, SystemDraftField field) {
    for (const auto& diagnostic : validation.diagnostics) {
        if (diagnostic.entity_kind == kind && diagnostic.entity_index == index &&
            diagnostic.field == field) {
            ImGui::TextColored(error_color, "%s", diagnostic.message.c_str());
        }
    }
}

void draw_profile_diagnostics(const SystemDraftBuildResult& validation,
                              DraftExecutionProfileKey key) {
    for (const auto& diagnostic : validation.diagnostics) {
        if (diagnostic.entity_kind == SystemDraftEntityKind::ExecutionProfile &&
            diagnostic.task_id == key.task_id && diagnostic.resource_id == key.resource_id) {
            ImGui::TextColored(error_color, "%s", diagnostic.message.c_str());
        }
    }
}

bool begin_properties(const char* identity) {
    return ImGui::BeginTable(identity, 2,
                             ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp);
}

void property_label(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0F);
}

void request_focus(SystemBuilderViewState& state, SystemBuilderFocusTarget target) {
    if (state.focus_request == target) {
        ImGui::SetKeyboardFocusHere();
        state.focus_request = SystemBuilderFocusTarget::None;
    }
}

std::optional<std::size_t> resource_index(const EditableSystemDraft& draft, ResourceId id) {
    const auto found = std::find_if(draft.resources().begin(), draft.resources().end(),
                                    [id](const auto& row) { return row.id == id; });
    if (found == draft.resources().end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(draft.resources().begin(), found));
}

std::optional<std::size_t> task_index(const EditableSystemDraft& draft, TaskId id) {
    const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                    [id](const auto& row) { return row.id == id; });
    if (found == draft.tasks().end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(draft.tasks().begin(), found));
}

std::optional<std::size_t> route_index(const EditableSystemDraft& draft, DraftMessageRouteKey key) {
    const auto found =
        std::find_if(draft.routes().begin(), draft.routes().end(), [key](const auto& row) {
            return row.source_task_id == key.source_task_id &&
                   row.destination_task_id == key.destination_task_id;
        });
    if (found == draft.routes().end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(draft.routes().begin(), found));
}

std::string task_label(const EditableSystemDraft& draft, TaskId id) {
    const auto index = task_index(draft, id);
    const auto name = index.has_value() ? draft.tasks()[*index].name : std::string{"Unknown"};
    return name + " (T" + std::to_string(id.value()) + ')';
}

std::string resource_label(const EditableSystemDraft& draft, ResourceId id) {
    const auto index = resource_index(draft, id);
    const auto name = index.has_value() ? draft.resources()[*index].name : std::string{"Unknown"};
    return name + " (R" + std::to_string(id.value()) + ')';
}

void draw_system(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                 std::string_view project_name) {
    ImGui::Text("Project: %.*s", static_cast<int>(project_name.size()), project_name.data());
    if (!begin_properties("system properties")) {
        return;
    }
    property_label("Tick period (ns)");
    auto tick_period = draft.tick_period_ns();
    if (ImGui::InputScalar("##tick-period", ImGuiDataType_S64, &tick_period)) {
        draft.set_tick_period_ns(tick_period);
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::System, 0,
                           SystemDraftField::TickPeriod);

    property_label("Preemption mode");
    const auto mode = draft.preemption_mode();
    const auto preview = mode == PreemptionMode::Preemptive ? "Preemptive" : "Non-preemptive";
    if (ImGui::BeginCombo("##preemption", preview)) {
        if (ImGui::Selectable("Preemptive", mode == PreemptionMode::Preemptive)) {
            draft.set_preemption_mode(PreemptionMode::Preemptive);
        }
        if (ImGui::Selectable("Non-preemptive", mode == PreemptionMode::NonPreemptive)) {
            draft.set_preemption_mode(PreemptionMode::NonPreemptive);
        }
        ImGui::EndCombo();
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::System, 0,
                           SystemDraftField::PreemptionMode);
    ImGui::EndTable();
}

void draw_resource_overview(const EditableSystemDraft& draft) {
    ImGui::Text("%zu resource(s)", draft.resources().size());
    if (ImGui::BeginTable("resource overview", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableHeadersRow();
        for (const auto& resource : draft.resources()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("R%llu", static_cast<unsigned long long>(resource.id.value()));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(resource.name.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Right-click Resources in Explorer to add an entity.");
}

void draw_resource(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                   std::vector<DraftTaskAssignment>& assignments, StructuralSelection& selection,
                   SystemBuilderViewState& state) {
    const auto index = resource_index(draft, *selection.resource_id());
    if (!index.has_value()) {
        return;
    }
    const auto resource = draft.resources()[*index];
    if (!begin_properties("resource properties")) {
        return;
    }
    property_label("Name");
    auto name = text_buffer<256>(resource.name);
    request_focus(state, SystemBuilderFocusTarget::ResourceName);
    if (ImGui::InputText("##resource-name", name.data(), name.size())) {
        draft.set_resource_name(*index, name.data());
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::Resource, *index,
                           SystemDraftField::Name);
    property_label("ID");
    auto id = resource.id.value();
    if (ImGui::InputScalar("##resource-id", ImGuiDataType_U64, &id)) {
        for (auto& assignment : assignments) {
            if (assignment.resource_id == resource.id) {
                assignment.resource_id = ResourceId{id};
            }
        }
        draft.set_resource_id(*index, ResourceId{id});
        selection.select_resource(ResourceId{id});
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::Resource, *index,
                           SystemDraftField::Id);
    ImGui::EndTable();
}

void draw_task_overview(const EditableSystemDraft& draft) {
    ImGui::Text("%zu task(s)", draft.tasks().size());
    if (ImGui::BeginTable("task overview", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Period");
        ImGui::TableHeadersRow();
        for (const auto& task : draft.tasks()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("T%llu", static_cast<unsigned long long>(task.id.value()));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(task.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%lld", static_cast<long long>(task.period));
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Right-click Tasks in Explorer to add an entity.");
}

void draw_task(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
               std::vector<DraftTaskAssignment>& assignments, StructuralSelection& selection,
               SystemBuilderViewState& state, bool protected_identity) {
    const auto index = task_index(draft, *selection.task_id());
    if (!index.has_value()) {
        return;
    }
    const auto task = draft.tasks()[*index];
    auto period = task.period;
    auto deadline = task.deadline;
    auto offset = task.offset;
    auto priority = task.priority;
    if (!begin_properties("task properties")) {
        return;
    }
    property_label("Name");
    auto name = text_buffer<256>(task.name);
    request_focus(state, SystemBuilderFocusTarget::TaskName);
    ImGui::BeginDisabled(protected_identity);
    if (ImGui::InputText("##task-name", name.data(), name.size())) {
        draft.set_task_name(*index, name.data());
    }
    ImGui::EndDisabled();
    draw_field_diagnostics(validation, SystemDraftEntityKind::Task, *index, SystemDraftField::Name);
    property_label("ID");
    auto id = task.id.value();
    ImGui::BeginDisabled(protected_identity);
    if (ImGui::InputScalar("##task-id", ImGuiDataType_U64, &id)) {
        const auto assignment =
            std::find_if(assignments.begin(), assignments.end(),
                         [&task](const auto& row) { return row.task_id == task.id; });
        if (assignment != assignments.end()) {
            assignment->task_id = TaskId{id};
        }
        draft.set_task_id(*index, TaskId{id});
        selection.select_task(TaskId{id});
    }
    ImGui::EndDisabled();
    draw_field_diagnostics(validation, SystemDraftEntityKind::Task, *index, SystemDraftField::Id);
    const auto timing_input = [&](const char* label, const char* identity, Tick& value,
                                  SystemDraftField field) {
        property_label(label);
        if (ImGui::InputScalar(identity, ImGuiDataType_S64, &value)) {
            draft.set_task_timing(
                *index, {.period = period, .deadline = deadline, .offset = offset}, priority);
        }
        draw_field_diagnostics(validation, SystemDraftEntityKind::Task, *index, field);
    };
    timing_input("Period", "##period", period, SystemDraftField::Period);
    timing_input("Deadline", "##deadline", deadline, SystemDraftField::Deadline);
    timing_input("Offset", "##offset", offset, SystemDraftField::Offset);
    property_label("Priority");
    if (ImGui::InputScalar("##priority", ImGuiDataType_S32, &priority)) {
        draft.set_task_timing(*index, {.period = period, .deadline = deadline, .offset = offset},
                              priority);
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::Task, *index,
                           SystemDraftField::TaskPriority);
    if (protected_identity) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("This task identity is fixed by the Bosch FMU interface. Timing, "
                            "execution profile, and allocation remain editable.");
    }
    ImGui::EndTable();
}

void draw_profile_matrix(EditableSystemDraft& draft, const SystemDraftBuildResult& validation) {
    if (draft.resources().empty() || draft.tasks().empty()) {
        ImGui::TextDisabled("Add at least one task and resource to define profiles.");
        return;
    }
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("profile matrix", static_cast<int>(draft.resources().size() + 1), flags,
                           ImVec2{0.0F, 16.0F * ImGui::GetTextLineHeightWithSpacing()})) {
        return;
    }
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("Task");
    for (const auto& resource : draft.resources()) {
        ImGui::TableSetupColumn(resource.name.c_str());
    }
    ImGui::TableHeadersRow();
    for (const auto& task : draft.tasks()) {
        ImGui::PushID(static_cast<int>(task.id.value()));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(task.name.c_str());
        for (const auto& resource : draft.resources()) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(resource.id.value()));
            auto execution = draft.execution_profile(task.id, resource.id);
            auto enabled = execution.has_value();
            if (ImGui::Checkbox("##enabled", &enabled)) {
                draft.set_execution_profile(task.id, resource.id,
                                            enabled ? std::optional<Tick>{1} : std::nullopt);
                execution = enabled ? std::optional<Tick>{1} : std::nullopt;
            }
            if (execution.has_value()) {
                ImGui::SameLine();
                auto ticks = *execution;
                ImGui::SetNextItemWidth(7.0F * ImGui::GetFontSize());
                if (ImGui::InputScalar("##ticks", ImGuiDataType_S64, &ticks)) {
                    draft.set_execution_profile(task.id, resource.id, ticks);
                }
                draw_profile_diagnostics(validation, {task.id, resource.id});
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

bool profile_pair_used(const EditableSystemDraft& draft, DraftExecutionProfileKey candidate,
                       DraftExecutionProfileKey current) {
    return candidate != current &&
           draft.execution_profile(candidate.task_id, candidate.resource_id).has_value();
}

void draw_profile(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                  StructuralSelection& selection, SystemBuilderViewState& state) {
    const auto key = *selection.execution_profile();
    auto execution = draft.execution_profile(key.task_id, key.resource_id);
    if (!execution.has_value()) {
        return;
    }
    auto edited = key;
    if (!begin_properties("profile properties")) {
        return;
    }
    property_label("Task");
    if (ImGui::BeginCombo("##profile-task", task_label(draft, edited.task_id).c_str())) {
        for (const auto& task : draft.tasks()) {
            const DraftExecutionProfileKey candidate{task.id, edited.resource_id};
            const auto unavailable = profile_pair_used(draft, candidate, key);
            ImGui::BeginDisabled(unavailable);
            if (ImGui::Selectable(task_label(draft, task.id).c_str(), task.id == edited.task_id)) {
                edited.task_id = task.id;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    property_label("Resource");
    if (ImGui::BeginCombo("##profile-resource",
                          resource_label(draft, edited.resource_id).c_str())) {
        for (const auto& resource : draft.resources()) {
            const DraftExecutionProfileKey candidate{edited.task_id, resource.id};
            const auto unavailable = profile_pair_used(draft, candidate, key);
            ImGui::BeginDisabled(unavailable);
            if (ImGui::Selectable(resource_label(draft, resource.id).c_str(),
                                  resource.id == edited.resource_id)) {
                edited.resource_id = resource.id;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    property_label("Execution ticks");
    auto ticks = *execution;
    request_focus(state, SystemBuilderFocusTarget::ProfileExecutionTime);
    if (ImGui::InputScalar("##execution-time", ImGuiDataType_S64, &ticks)) {
        execution = ticks;
    }
    if (edited != key) {
        static_cast<void>(draft.remove_execution_profile(key));
        draft.set_execution_profile(edited.task_id, edited.resource_id, execution);
        selection.select_execution_profile(edited);
    } else if (ticks != *draft.execution_profile(key.task_id, key.resource_id)) {
        draft.set_execution_profile(key.task_id, key.resource_id, ticks);
    }
    draw_profile_diagnostics(validation, key);
    ImGui::EndTable();
}

void draw_route_table(const EditableSystemDraft& draft) {
    if (ImGui::BeginTable("route overview", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Destination");
        ImGui::TableSetupColumn("Send offset");
        ImGui::TableSetupColumn("Delay");
        ImGui::TableHeadersRow();
        for (const auto& route : draft.routes()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(task_label(draft, route.source_task_id).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(task_label(draft, route.destination_task_id).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%lld", static_cast<long long>(route.send_offset));
            ImGui::TableNextColumn();
            ImGui::Text("%lld", static_cast<long long>(route.delay));
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Right-click Message Routes in Explorer to add an entity.");
}

bool route_pair_used(const EditableSystemDraft& draft, DraftMessageRouteKey candidate,
                     DraftMessageRouteKey current) {
    return candidate != current &&
           std::any_of(draft.routes().begin(), draft.routes().end(), [candidate](const auto& row) {
               return row.source_task_id == candidate.source_task_id &&
                      row.destination_task_id == candidate.destination_task_id;
           });
}

void draw_route(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                StructuralSelection& selection, SystemBuilderViewState& state,
                bool protected_endpoints) {
    const auto key = *selection.message_route();
    const auto index = route_index(draft, key);
    if (!index.has_value()) {
        return;
    }
    auto route = draft.routes()[*index];
    auto edited_key = key;
    if (!begin_properties("route properties")) {
        return;
    }
    property_label("Source task");
    request_focus(state, SystemBuilderFocusTarget::RouteSource);
    ImGui::BeginDisabled(protected_endpoints);
    if (ImGui::BeginCombo("##route-source", task_label(draft, route.source_task_id).c_str())) {
        for (const auto& task : draft.tasks()) {
            const DraftMessageRouteKey candidate{task.id, edited_key.destination_task_id};
            ImGui::BeginDisabled(route_pair_used(draft, candidate, key));
            if (ImGui::Selectable(task_label(draft, task.id).c_str(),
                                  task.id == route.source_task_id)) {
                route.source_task_id = task.id;
                edited_key.source_task_id = task.id;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    property_label("Destination task");
    ImGui::BeginDisabled(protected_endpoints);
    if (ImGui::BeginCombo("##route-destination",
                          task_label(draft, route.destination_task_id).c_str())) {
        for (const auto& task : draft.tasks()) {
            const DraftMessageRouteKey candidate{edited_key.source_task_id, task.id};
            ImGui::BeginDisabled(route_pair_used(draft, candidate, key));
            if (ImGui::Selectable(task_label(draft, task.id).c_str(),
                                  task.id == route.destination_task_id)) {
                route.destination_task_id = task.id;
                edited_key.destination_task_id = task.id;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    property_label("Send offset");
    ImGui::InputScalar("##send-offset", ImGuiDataType_S64, &route.send_offset);
    draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, *index,
                           SystemDraftField::SendOffset);
    property_label("Delay");
    ImGui::InputScalar("##delay", ImGuiDataType_S64, &route.delay);
    draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, *index,
                           SystemDraftField::Delay);
    if (route != draft.routes()[*index]) {
        draft.set_message_route(*index, route);
        selection.select_message_route(edited_key);
    }
    if (protected_endpoints) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled(
            "Bosch network route endpoints are fixed; send offset and delay remain editable.");
    }
    ImGui::EndTable();
}

void draw_validation(const SystemDraftBuildResult& validation) {
    if (validation.valid()) {
        ImGui::TextColored(ImVec4{0.45F, 0.85F, 0.55F, 1.0F}, "System draft is valid.");
        return;
    }
    ImGui::TextColored(error_color, "%zu issue(s)", validation.diagnostics.size());
    for (const auto& diagnostic : validation.diagnostics) {
        ImGui::BulletText("%s", diagnostic.message.c_str());
    }
}

void draw_connection(const EditableSystemDraft& draft, const StructuralSelection& selection,
                     ProjectSystemEditPolicy edit_policy) {
    const auto connection = selection.connection();
    if (!connection.has_value() || !begin_properties("connection properties")) {
        return;
    }
    property_label("Source");
    ImGui::TextUnformatted(task_label(draft, connection->source_task_id).c_str());
    property_label("Destination");
    ImGui::TextUnformatted(task_label(draft, connection->destination_task_id).c_str());
    property_label("Kind");
    ImGui::TextUnformatted(connection->kind == GuiConnectionKind::Logical ? "Logical"
                                                                          : "Communication");
    property_label("Displayed latency");
    Tick latency = 0;
    if (connection->kind == GuiConnectionKind::Communication) {
        const auto index = route_index(
            draft, {connection->source_task_id, connection->destination_task_id});
        if (edit_policy == ProjectSystemEditPolicy::BoschCompatible) {
            latency = 80;
        } else if (index.has_value()) {
            latency = draft.routes()[*index].delay;
        }
    }
    ImGui::Text("%lld ticks", static_cast<long long>(latency));
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped(connection->kind == GuiConnectionKind::Logical
                           ? "Logical dependencies are presentation-only and create no messages, delays, or canonical network events."
                           : "Communication latency describes the visible network delay. The Bosch adapter's internal one-tick send handoff is intentionally hidden.");
    ImGui::EndTable();
}

} // namespace

void draw_system_builder(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                         std::vector<DraftTaskAssignment>& assignments,
                         StructuralSelection& selection,
                         const ExperimentPresentationSnapshot&, bool editing_enabled,
                         ProjectSystemEditPolicy edit_policy, std::string_view project_name,
                         SystemBuilderViewState& state) {
    ImGui::BeginDisabled(!editing_enabled);
    switch (selection.kind()) {
    case StructuralSelectionKind::System:
        draw_system(draft, validation, project_name);
        break;
    case StructuralSelectionKind::Section:
        switch (*selection.section()) {
        case StructuralSection::Resources:
            draw_resource_overview(draft);
            break;
        case StructuralSection::Tasks:
            draw_task_overview(draft);
            break;
        case StructuralSection::ExecutionProfiles:
            draw_profile_matrix(draft, validation);
            break;
        case StructuralSection::MessageRoutes:
            draw_route_table(draft);
            break;
        }
        break;
    case StructuralSelectionKind::Resource:
        draw_resource(draft, validation, assignments, selection, state);
        break;
    case StructuralSelectionKind::Task:
        draw_task(draft, validation, assignments, selection, state,
                  edit_policy == ProjectSystemEditPolicy::BoschCompatible);
        break;
    case StructuralSelectionKind::ExecutionProfile:
        draw_profile(draft, validation, selection, state);
        break;
    case StructuralSelectionKind::MessageRoute:
        draw_route(draft, validation, selection, state,
                   edit_policy == ProjectSystemEditPolicy::BoschCompatible);
        break;
    case StructuralSelectionKind::Connection:
        draw_connection(draft, selection, edit_policy);
        break;
    }
    ImGui::EndDisabled();
    if (!editing_enabled) {
        ImGui::TextWrapped("Pause the simulation to edit the system.");
    }
    ImGui::SeparatorText("Validation");
    draw_validation(validation);
}

} // namespace cpssim::gui
