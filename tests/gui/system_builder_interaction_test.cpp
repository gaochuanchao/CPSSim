/*** Verify headless Explorer lifecycle, focus intent, and confirmed cascades. ***/

#include "cpssim/gui/system_builder_interaction.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace cpssim;

TEST_CASE("Explorer creation selects scrolls and focuses the new structural entity",
          "[gui][system-builder][explorer]") {
    auto draft = EditableSystemDraft::minimal();
    StructuralSelection selection;
    SystemExplorerInteraction interaction;

    const auto resource = interaction.create(StructuralSection::Resources, draft, selection);
    REQUIRE(resource.changed);
    REQUIRE((selection.resource_id() == ResourceId{2}));
    REQUIRE((resource.expand_section == StructuralSection::Resources));
    REQUIRE((resource.scroll_to == selection));
    REQUIRE((resource.focus == SystemBuilderFocusTarget::ResourceName));

    const auto task = interaction.create(StructuralSection::Tasks, draft, selection);
    REQUIRE(task.changed);
    REQUIRE((selection.task_id() == TaskId{2}));
    REQUIRE((task.focus == SystemBuilderFocusTarget::TaskName));

    const auto profile =
        interaction.create(StructuralSection::ExecutionProfiles, draft, selection);
    REQUIRE(profile.changed);
    REQUIRE((selection.execution_profile() ==
             DraftExecutionProfileKey{TaskId{1}, ResourceId{2}}));
    REQUIRE((profile.focus == SystemBuilderFocusTarget::ProfileExecutionTime));
}

TEST_CASE("Explorer reports why profile and route creation is unavailable",
          "[gui][system-builder][explorer][disabled]") {
    auto draft = EditableSystemDraft::minimal();
    SystemExplorerInteraction interaction;

    const auto profile =
        interaction.create_availability(StructuralSection::ExecutionProfiles, draft);
    REQUIRE_FALSE(profile.available);
    REQUIRE_FALSE(profile.explanation.empty());

    REQUIRE(interaction.create_availability(StructuralSection::MessageRoutes, draft).available);
    StructuralSelection selection;
    REQUIRE(interaction.create(StructuralSection::MessageRoutes, draft, selection).changed);
    const auto route = interaction.create_availability(StructuralSection::MessageRoutes, draft);
    REQUIRE_FALSE(route.available);
    REQUIRE_FALSE(route.explanation.empty());
}

TEST_CASE("Explorer duplication copies only the selected entity values",
          "[gui][system-builder][explorer][duplicate]") {
    auto draft = EditableSystemDraft::minimal();
    const auto second_resource = draft.add_resource();
    StructuralSelection selected;
    selected.select_execution_profile({TaskId{1}, ResourceId{1}});
    StructuralSelection result_selection;
    SystemExplorerInteraction interaction;

    const auto result = interaction.duplicate(selected, draft, result_selection);
    REQUIRE(result.changed);
    REQUIRE((result_selection.execution_profile() ==
             DraftExecutionProfileKey{TaskId{1}, second_resource}));
    REQUIRE((draft.execution_profile(TaskId{1}, second_resource) == 1));
}

TEST_CASE("cascade confirmation cancel is inert and confirm repairs selection",
          "[gui][system-builder][explorer][delete]") {
    auto draft = EditableSystemDraft::minimal();
    const auto second_task = draft.add_task();
    draft.set_execution_profile(second_task, ResourceId{1}, 2);
    static_cast<void>(draft.add_message_route(TaskId{1}, second_task));
    std::vector<DraftTaskAssignment> assignments{
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}},
        {.task_id = second_task, .resource_id = ResourceId{1}}};
    StructuralSelection selection;
    selection.select_task(TaskId{1});
    SystemExplorerInteraction interaction;

    REQUIRE(interaction.request_delete(selection, draft, assignments));
    REQUIRE((interaction.pending_delete()->structural.execution_profiles == 1));
    REQUIRE((interaction.pending_delete()->structural.outgoing_routes == 1));
    REQUIRE((interaction.pending_delete()->run_plan_assignments == 1));
    interaction.cancel_delete();
    REQUIRE((draft.tasks().size() == 2));
    REQUIRE((assignments.size() == 2));

    REQUIRE(interaction.request_delete(selection, draft, assignments));
    const auto deleted = interaction.confirm_delete(draft, assignments, selection);
    REQUIRE(deleted.changed);
    REQUIRE((draft.tasks().size() == 1));
    REQUIRE((draft.tasks()[0].id == second_task));
    REQUIRE((assignments.size() == 1));
    REQUIRE((selection.task_id() == second_task));
    REQUIRE(draft.routes().empty());
}

TEST_CASE("resource cascade clears affected draft assignments without touching tasks",
          "[gui][system-builder][explorer][delete]") {
    auto draft = EditableSystemDraft::minimal();
    std::vector<DraftTaskAssignment> assignments{
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}}};
    StructuralSelection selection;
    selection.select_resource(ResourceId{1});
    SystemExplorerInteraction interaction;

    REQUIRE(interaction.request_delete(selection, draft, assignments));
    REQUIRE((interaction.pending_delete()->structural.execution_profiles == 1));
    REQUIRE((interaction.pending_delete()->run_plan_assignments == 1));
    REQUIRE(interaction.confirm_delete(draft, assignments, selection).changed);
    REQUIRE(draft.resources().empty());
    REQUIRE(draft.profiles().empty());
    REQUIRE_FALSE(assignments[0].resource_id.has_value());
    REQUIRE((selection.section() == StructuralSection::Resources));
}

} // namespace
