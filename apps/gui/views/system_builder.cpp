/*** Render Goal 2 structured system editing without mutating active runtime. ***/

#include "system_builder.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace cpssim::gui {
namespace {

constexpr ImVec4 error_color{1.0F, 0.48F, 0.32F, 1.0F};
constexpr ImVec4 valid_color{0.45F, 0.85F, 0.55F, 1.0F};
constexpr std::size_t profile_page_size = 12;

void draw_field_diagnostics(const SystemDraftBuildResult& validation, SystemDraftEntityKind kind,
                            std::size_t index, SystemDraftField field) {
    for (const auto& diagnostic : validation.diagnostics) {
        if (diagnostic.entity_kind == kind && diagnostic.entity_index == index &&
            diagnostic.field == field) {
            ImGui::TextColored(error_color, "%s", diagnostic.message.c_str());
        }
    }
}

void draw_profile_diagnostics(const SystemDraftBuildResult& validation, TaskId task_id,
                              ResourceId resource_id) {
    for (const auto& diagnostic : validation.diagnostics) {
        if (diagnostic.entity_kind == SystemDraftEntityKind::ExecutionProfile &&
            diagnostic.task_id == task_id && diagnostic.resource_id == resource_id) {
            ImGui::TextColored(error_color, "! %s", diagnostic.message.c_str());
        }
    }
}

template <std::size_t Size> std::array<char, Size> text_buffer(const std::string& value) {
    std::array<char, Size> result{};
    std::copy_n(value.begin(), std::min(value.size(), result.size() - 1), result.begin());
    return result;
}

std::string task_label(const EditableSystemDraft& draft, TaskId id) {
    const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                    [id](const auto& task) { return task.id == id; });
    const auto name = found == draft.tasks().end() ? std::string{"Unknown"} : found->name;
    return name + " (T" + std::to_string(id.value()) + ')';
}

std::string resource_label(const EditableSystemDraft& draft, ResourceId id) {
    const auto found = std::find_if(draft.resources().begin(), draft.resources().end(),
                                    [id](const auto& resource) { return resource.id == id; });
    const auto name = found == draft.resources().end() ? std::string{"Unknown"} : found->name;
    return name + " (R" + std::to_string(id.value()) + ')';
}

void synchronize_assignments(const EditableSystemDraft& draft,
                             std::vector<DraftTaskAssignment>& assignments) {
    std::vector<DraftTaskAssignment> synchronized;
    synchronized.reserve(draft.tasks().size());
    for (const auto& task : draft.tasks()) {
        const auto found =
            std::find_if(assignments.begin(), assignments.end(),
                         [&task](const auto& assignment) { return assignment.task_id == task.id; });
        synchronized.push_back(
            {.task_id = task.id,
             .resource_id = found == assignments.end() ? std::nullopt : found->resource_id});
    }
    assignments = std::move(synchronized);
}

bool draw_task_selector(const char* label, const EditableSystemDraft& draft, TaskId& selected) {
    auto changed = false;
    const auto preview = task_label(draft, selected);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const auto& task : draft.tasks()) {
            const auto item_label = task_label(draft, task.id);
            const auto is_selected = task.id == selected;
            if (ImGui::Selectable(item_label.c_str(), is_selected)) {
                selected = task.id;
                changed = true;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void draw_general(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                  std::string_view project_name) {
    ImGui::Text("Project: %.*s", static_cast<int>(project_name.size()), project_name.data());
    ImGui::TextDisabled("The project name is edited by Save Project As.");

    auto tick_period = draft.tick_period_ns();
    if (ImGui::InputScalar("Tick period (ns)", ImGuiDataType_S64, &tick_period)) {
        draft.set_tick_period_ns(tick_period);
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::System, 0,
                           SystemDraftField::TickPeriod);

    const auto preemption = draft.preemption_mode();
    const auto preview = preemption == PreemptionMode::Preemptive ? "Preemptive" : "Non-preemptive";
    if (ImGui::BeginCombo("Preemption", preview)) {
        if (ImGui::Selectable("Preemptive", preemption == PreemptionMode::Preemptive)) {
            draft.set_preemption_mode(PreemptionMode::Preemptive);
        }
        if (ImGui::Selectable("Non-preemptive", preemption == PreemptionMode::NonPreemptive)) {
            draft.set_preemption_mode(PreemptionMode::NonPreemptive);
        }
        ImGui::EndCombo();
    }
    draw_field_diagnostics(validation, SystemDraftEntityKind::System, 0,
                           SystemDraftField::PreemptionMode);
}

void draw_resources(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                    SystemBuilderViewState& state) {
    std::optional<std::size_t> duplicate;
    std::optional<std::size_t> remove;
    if (ImGui::BeginTable("System resources", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed,
                                7.0F * ImGui::GetFontSize());
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Duplicate", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < draft.resources().size(); ++index) {
            const auto resource = draft.resources()[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            auto id = resource.id.value();
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputScalar("##resource-id", ImGuiDataType_U64, &id)) {
                draft.set_resource_id(index, ResourceId{id});
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Resource, index,
                                   SystemDraftField::Id);
            ImGui::TableSetColumnIndex(1);
            auto name = text_buffer<256>(resource.name);
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputText("##resource-name", name.data(), name.size())) {
                draft.set_resource_name(index, name.data());
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Resource, index,
                                   SystemDraftField::Name);
            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("Duplicate")) {
                duplicate = index;
            }
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("Remove")) {
                remove = index;
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Resource, index,
                                   SystemDraftField::Collection);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (duplicate.has_value()) {
        draft.duplicate_resource(*duplicate);
        state.status = "Resource duplicated with a new stable ID.";
        state.status_error = false;
    }
    if (remove.has_value()) {
        const auto result = draft.remove_resource(*remove);
        if (result.diagnostic.has_value()) {
            state.status = result.diagnostic->message;
            state.status_error = true;
        } else {
            state.status = "Resource removed.";
            state.status_error = false;
        }
    }
    if (ImGui::Button("Add resource")) {
        draft.add_resource();
    }
}

void draw_tasks(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                SystemBuilderViewState& state) {
    std::optional<std::size_t> duplicate;
    std::optional<std::size_t> remove;
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                       ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("System tasks", 9, flags, ImVec2{0.0F, 0.0F})) {
        ImGui::TableSetupScrollFreeze(2, 1);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                13.0F * ImGui::GetFontSize());
        ImGui::TableSetupColumn("Period");
        ImGui::TableSetupColumn("Deadline");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Priority");
        ImGui::TableSetupColumn("Duplicate");
        ImGui::TableSetupColumn("Remove");
        ImGui::TableSetupColumn("Issues");
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < draft.tasks().size(); ++index) {
            const auto task = draft.tasks()[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            auto id = task.id.value();
            ImGui::SetNextItemWidth(7.0F * ImGui::GetFontSize());
            if (ImGui::InputScalar("##task-id", ImGuiDataType_U64, &id)) {
                draft.set_task_id(index, TaskId{id});
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Id);
            ImGui::TableSetColumnIndex(1);
            auto name = text_buffer<256>(task.name);
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputText("##task-name", name.data(), name.size())) {
                draft.set_task_name(index, name.data());
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Name);

            auto period = task.period;
            auto deadline = task.deadline;
            auto offset = task.offset;
            auto priority = task.priority;
            ImGui::TableSetColumnIndex(2);
            if (ImGui::InputScalar("##period", ImGuiDataType_S64, &period)) {
                draft.set_task_timing(
                    index, {.period = period, .deadline = deadline, .offset = offset}, priority);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Period);
            ImGui::TableSetColumnIndex(3);
            if (ImGui::InputScalar("##deadline", ImGuiDataType_S64, &deadline)) {
                draft.set_task_timing(
                    index, {.period = period, .deadline = deadline, .offset = offset}, priority);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Deadline);
            ImGui::TableSetColumnIndex(4);
            if (ImGui::InputScalar("##offset", ImGuiDataType_S64, &offset)) {
                draft.set_task_timing(
                    index, {.period = period, .deadline = deadline, .offset = offset}, priority);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Offset);
            ImGui::TableSetColumnIndex(5);
            if (ImGui::InputScalar("##priority", ImGuiDataType_S32, &priority)) {
                draft.set_task_timing(
                    index, {.period = period, .deadline = deadline, .offset = offset}, priority);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::TaskPriority);
            ImGui::TableSetColumnIndex(6);
            if (ImGui::SmallButton("Duplicate")) {
                duplicate = index;
            }
            ImGui::TableSetColumnIndex(7);
            if (ImGui::SmallButton("Remove")) {
                remove = index;
            }
            ImGui::TableSetColumnIndex(8);
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::ExecutionTime);
            draw_field_diagnostics(validation, SystemDraftEntityKind::Task, index,
                                   SystemDraftField::Collection);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (duplicate.has_value()) {
        draft.duplicate_task(*duplicate);
        state.status = "Task and its execution profiles duplicated with a new stable ID.";
        state.status_error = false;
    }
    if (remove.has_value()) {
        const auto result = draft.remove_task(*remove);
        if (result.diagnostic.has_value()) {
            state.status = result.diagnostic->message;
            state.status_error = true;
        } else {
            state.status = "Task removed.";
            state.status_error = false;
        }
    }
    if (ImGui::Button("Add task")) {
        draft.add_task();
    }
}

void draw_default_assignments(EditableSystemDraft& draft,
                              std::vector<DraftTaskAssignment>& assignments) {
    ImGui::SeparatorText("Default run assignments");
    ImGui::TextDisabled(
        "Assignments are explicit run inputs and are validated with the edited system on Apply.");
    for (auto& assignment : assignments) {
        const auto selected = assignment.resource_id;
        const auto preview = selected.has_value() ? resource_label(draft, *selected)
                                                  : std::string{"Select resource..."};
        ImGui::PushID(&assignment);
        ImGui::TextUnformatted(task_label(draft, assignment.task_id).c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(16.0F * ImGui::GetFontSize());
        if (ImGui::BeginCombo("##default-resource", preview.c_str())) {
            if (ImGui::Selectable("Unassigned", !selected.has_value())) {
                assignment.resource_id.reset();
            }
            for (const auto& profile : draft.profiles()) {
                if (profile.task_id != assignment.task_id) {
                    continue;
                }
                const auto label = resource_label(draft, profile.resource_id);
                const auto is_selected = assignment.resource_id == profile.resource_id;
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    assignment.resource_id = profile.resource_id;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (!assignment.resource_id.has_value()) {
            ImGui::TextColored(error_color, "A default resource assignment is required.");
        }
        ImGui::PopID();
    }
}

void draw_profiles(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                   std::vector<DraftTaskAssignment>& assignments, SystemBuilderViewState& state) {
    const auto resource_count = draft.resources().size();
    const auto page_count =
        std::max<std::size_t>(1, (resource_count + profile_page_size - 1) / profile_page_size);
    state.profile_resource_page = std::min(state.profile_resource_page, page_count - 1);
    ImGui::BeginDisabled(state.profile_resource_page == 0);
    if (ImGui::Button("Previous resources")) {
        --state.profile_resource_page;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("Resource page %zu/%zu", state.profile_resource_page + 1, page_count);
    ImGui::SameLine();
    ImGui::BeginDisabled(state.profile_resource_page + 1 >= page_count);
    if (ImGui::Button("Next resources")) {
        ++state.profile_resource_page;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Enable a cell, then enter positive deterministic execution ticks.");

    const auto first = state.profile_resource_page * profile_page_size;
    const auto last = std::min(first + profile_page_size, resource_count);
    const auto visible_resources = last - first;
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("Execution profile matrix", static_cast<int>(visible_resources + 1),
                          flags, ImVec2{0.0F, 18.0F * ImGui::GetTextLineHeightWithSpacing()})) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthFixed,
                                13.0F * ImGui::GetFontSize());
        for (std::size_t resource_index = first; resource_index < last; ++resource_index) {
            const auto& resource = draft.resources()[resource_index];
            const auto label = resource.name + "##R" + std::to_string(resource.id.value());
            ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_WidthFixed,
                                    13.0F * ImGui::GetFontSize());
        }
        ImGui::TableHeadersRow();
        for (const auto& task : draft.tasks()) {
            ImGui::PushID(&task);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s (T%llu)", task.name.c_str(),
                        static_cast<unsigned long long>(task.id.value()));
            for (std::size_t resource_index = first; resource_index < last; ++resource_index) {
                const auto& resource = draft.resources()[resource_index];
                ImGui::TableSetColumnIndex(static_cast<int>(resource_index - first + 1));
                ImGui::PushID(static_cast<int>(resource_index));
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
                } else {
                    ImGui::SameLine();
                    ImGui::TextDisabled("—");
                }
                draw_profile_diagnostics(validation, task.id, resource.id);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    draw_default_assignments(draft, assignments);
}

void draw_routes(EditableSystemDraft& draft, const SystemDraftBuildResult& validation) {
    std::optional<std::size_t> remove;
    if (ImGui::BeginTable("Message routes", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Destination");
        ImGui::TableSetupColumn("Send offset");
        ImGui::TableSetupColumn("Delay");
        ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Issues");
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < draft.routes().size(); ++index) {
            auto route = draft.routes()[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (draw_task_selector("##source", draft, route.source_task_id)) {
                draft.set_message_route(index, route);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, index,
                                   SystemDraftField::SourceTask);
            ImGui::TableSetColumnIndex(1);
            if (draw_task_selector("##destination", draft, route.destination_task_id)) {
                draft.set_message_route(index, route);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, index,
                                   SystemDraftField::DestinationTask);
            ImGui::TableSetColumnIndex(2);
            if (ImGui::InputScalar("##send-offset", ImGuiDataType_S64, &route.send_offset)) {
                draft.set_message_route(index, route);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, index,
                                   SystemDraftField::SendOffset);
            ImGui::TableSetColumnIndex(3);
            if (ImGui::InputScalar("##delay", ImGuiDataType_S64, &route.delay)) {
                draft.set_message_route(index, route);
            }
            draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, index,
                                   SystemDraftField::Delay);
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("Remove")) {
                remove = index;
            }
            ImGui::TableSetColumnIndex(5);
            draw_field_diagnostics(validation, SystemDraftEntityKind::MessageRoute, index,
                                   SystemDraftField::Collection);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (remove.has_value()) {
        draft.remove_message_route(*remove);
    }
    ImGui::BeginDisabled(draft.tasks().empty());
    if (ImGui::Button("Add message route")) {
        const auto source = draft.tasks().front().id;
        const auto destination = draft.tasks().size() > 1 ? draft.tasks()[1].id : source;
        draft.add_message_route(source, destination);
    }
    ImGui::EndDisabled();
}

void draw_validation(const SystemDraftBuildResult& validation) {
    if (validation.valid()) {
        ImGui::TextColored(valid_color, "System draft is valid.");
        return;
    }
    ImGui::TextColored(error_color, "%zu issue(s)", validation.diagnostics.size());
    for (const auto& diagnostic : validation.diagnostics) {
        ImGui::BulletText("%s", diagnostic.message.c_str());
    }
}

} // namespace

SystemBuilderAction draw_system_builder(EditableSystemDraft& draft,
                                        SystemDraftBuildResult& validation,
                                        std::vector<DraftTaskAssignment>& assignments,
                                        bool changes_dirty, std::string_view project_name,
                                        SystemBuilderViewState& state) {
    synchronize_assignments(draft, assignments);
    validation = draft.build();
    ImGui::Text("Draft: %s", changes_dirty ? "Modified (unapplied)" : "Matches applied system");
    ImGui::SameLine();
    if (ImGui::Button("Validate changes")) {
        validation = draft.build();
        state.status = validation.valid() ? "System draft is valid." : "System draft has errors.";
        state.status_error = !validation.valid();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!changes_dirty || !validation.valid());
    const auto apply = ImGui::Button("Apply and restart");
    ImGui::EndDisabled();

    if (!state.status.empty()) {
        ImGui::TextColored(state.status_error ? error_color : valid_color, "%s",
                           state.status.c_str());
    }
    ImGui::Separator();
    if (ImGui::BeginTabBar("System Builder sections")) {
        if (ImGui::BeginTabItem("General")) {
            draw_general(draft, validation, project_name);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Resources")) {
            draw_resources(draft, validation, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tasks")) {
            draw_tasks(draft, validation, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Execution Profiles")) {
            draw_profiles(draft, validation, assignments, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Message Routes")) {
            draw_routes(draft, validation);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Validation")) {
            draw_validation(validation);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    validation = draft.build();
    return apply ? SystemBuilderAction::ApplyAndRestart : SystemBuilderAction::None;
}

} // namespace cpssim::gui
