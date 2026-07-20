/*** Verify complete detached system editing and structured validation. ***/

#include "cpssim/config/json_config.hpp"
#include "cpssim/gui/editable_system_draft.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <string>

namespace {

using namespace cpssim;

ExperimentConfig make_config() {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::NonPreemptive},
        {ResourceSpec{ResourceId{4}, "local"}, ResourceSpec{ResourceId{9}, "cloud"}},
        {TaskSpec{TaskId{3}, "sensor", PeriodicTimingSpec{.period = 10, .deadline = 9, .offset = 1},
                  2},
         TaskSpec{TaskId{8}, "controller",
                  PeriodicTimingSpec{.period = 20, .deadline = 20, .offset = 0}, 1}},
        {{.task_id = TaskId{3}, .resource_id = ResourceId{4}, .execution_time = 2},
         {.task_id = TaskId{3}, .resource_id = ResourceId{9}, .execution_time = 3},
         {.task_id = TaskId{8}, .resource_id = ResourceId{9}, .execution_time = 4}},
        {{.source_task_id = TaskId{3},
          .destination_task_id = TaskId{8},
          .send_offset = 1,
          .delay = 5}}};
}

bool has_diagnostic(const SystemDraftBuildResult& result, SystemDraftDiagnosticCode code,
                    SystemDraftEntityKind kind, SystemDraftField field) {
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                       [=](const auto& diagnostic) {
                           return diagnostic.code == code && diagnostic.entity_kind == kind &&
                                  diagnostic.field == field;
                       });
}

TEST_CASE("system config round trips through an initially clean editable draft",
          "[gui][system-builder][draft]") {
    const auto original = make_config();
    const EditableSystemDraft draft{original};
    const auto result = draft.build();

    REQUIRE_FALSE(draft.dirty());
    REQUIRE(result.valid());
    REQUIRE((serialize_experiment_config_json(*result.config) ==
             serialize_experiment_config_json(original)));
}

TEST_CASE("minimal system draft is valid and allocates deterministic positive IDs",
          "[gui][system-builder][ids]") {
    auto first = EditableSystemDraft::minimal();
    auto second = EditableSystemDraft::minimal();

    REQUIRE(first.build().valid());
    REQUIRE((first.add_resource() == ResourceId{2}));
    REQUIRE((first.add_task() == TaskId{2}));
    REQUIRE((second.add_resource() == ResourceId{2}));
    REQUIRE((second.add_task() == TaskId{2}));
    REQUIRE(first.dirty());

    first.set_resource_id(1, ResourceId{7});
    REQUIRE((first.add_resource() == ResourceId{2}));
}

TEST_CASE("resource and task edits preserve IDs while duplicates copy profiles",
          "[gui][system-builder][mutation]") {
    auto draft = EditableSystemDraft{make_config()};
    draft.set_resource_name(0, "renamed local");
    draft.set_task_name(0, "renamed sensor");
    draft.set_task_timing(0, 12, 10, 2, 4);

    REQUIRE((draft.resources()[0].id == ResourceId{4}));
    REQUIRE((draft.tasks()[0].id == TaskId{3}));
    const auto resource_copy = draft.duplicate_resource(0);
    const auto task_copy = draft.duplicate_task(0);
    REQUIRE((resource_copy == ResourceId{1}));
    REQUIRE((task_copy == TaskId{1}));
    REQUIRE(draft.execution_profile(task_copy, ResourceId{4}).has_value());
    REQUIRE(draft.execution_profile(TaskId{3}, resource_copy).has_value());
    REQUIRE(draft.build().valid());
}

TEST_CASE("referenced entity removal is blocked consistently", "[gui][system-builder][removal]") {
    auto draft = EditableSystemDraft{make_config()};
    const auto resource_result = draft.remove_resource(0);
    const auto task_result = draft.remove_task(0);

    REQUIRE_FALSE(resource_result.changed);
    REQUIRE(resource_result.diagnostic.has_value());
    REQUIRE((resource_result.diagnostic->code == SystemDraftDiagnosticCode::ReferencedEntity));
    REQUIRE_FALSE(task_result.changed);
    REQUIRE(task_result.diagnostic.has_value());

    draft.set_execution_profile(TaskId{3}, ResourceId{4}, std::nullopt);
    draft.set_execution_profile(TaskId{3}, ResourceId{9}, std::nullopt);
    draft.remove_message_route(0);
    REQUIRE(draft.remove_task(0).changed);
    REQUIRE(draft.remove_resource(0).changed);
}

TEST_CASE("system draft reports field-addressed timing ID and name diagnostics",
          "[gui][system-builder][validation]") {
    auto draft = EditableSystemDraft{make_config()};
    draft.set_tick_period_ns(0);
    draft.set_resource_id(1, ResourceId{4});
    draft.set_resource_name(1, "local");
    draft.set_task_id(1, TaskId{3});
    draft.set_task_name(1, "sensor");
    draft.set_task_timing(0, 0, 0, -1, -1);
    const auto result = draft.build();

    REQUIRE_FALSE(result.valid());
    REQUIRE(has_diagnostic(result, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::System, SystemDraftField::TickPeriod));
    REQUIRE(has_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                           SystemDraftEntityKind::Resource, SystemDraftField::Id));
    REQUIRE(has_diagnostic(result, SystemDraftDiagnosticCode::Duplicate,
                           SystemDraftEntityKind::Task, SystemDraftField::Name));
    REQUIRE(has_diagnostic(result, SystemDraftDiagnosticCode::Negative, SystemDraftEntityKind::Task,
                           SystemDraftField::Offset));
}

TEST_CASE("profile matrix conversion rejects missing duplicate and invalid profiles",
          "[gui][system-builder][profiles]") {
    auto draft = EditableSystemDraft{make_config()};
    draft.set_execution_profile(TaskId{8}, ResourceId{9}, std::nullopt);
    auto missing = draft.build();
    REQUIRE(has_diagnostic(missing, SystemDraftDiagnosticCode::Required,
                           SystemDraftEntityKind::Task, SystemDraftField::ExecutionTime));

    draft.set_execution_profile(TaskId{8}, ResourceId{9}, 30);
    draft.append_execution_profile(
        {.task_id = TaskId{8}, .resource_id = ResourceId{9}, .execution_time = 4});
    draft.append_execution_profile(
        {.task_id = TaskId{99}, .resource_id = ResourceId{77}, .execution_time = 1});
    const auto invalid = draft.build();
    REQUIRE(has_diagnostic(invalid, SystemDraftDiagnosticCode::ExecutionExceedsDeadline,
                           SystemDraftEntityKind::ExecutionProfile,
                           SystemDraftField::ExecutionTime));
    REQUIRE(has_diagnostic(invalid, SystemDraftDiagnosticCode::Duplicate,
                           SystemDraftEntityKind::ExecutionProfile,
                           SystemDraftField::ExecutionTime));
    REQUIRE(has_diagnostic(invalid, SystemDraftDiagnosticCode::UnknownReference,
                           SystemDraftEntityKind::ExecutionProfile,
                           SystemDraftField::TaskReference));
}

TEST_CASE("message route editing converts valid rows and maps invalid duplicates",
          "[gui][system-builder][routes]") {
    auto draft = EditableSystemDraft{make_config()};
    const auto duplicate = draft.add_message_route(TaskId{3}, TaskId{8});
    draft.set_message_route(duplicate, {.source_task_id = TaskId{3},
                                        .destination_task_id = TaskId{8},
                                        .send_offset = 0,
                                        .delay = -1});
    const auto invalid = draft.build();
    REQUIRE(has_diagnostic(invalid, SystemDraftDiagnosticCode::Duplicate,
                           SystemDraftEntityKind::MessageRoute, SystemDraftField::Collection));
    REQUIRE(has_diagnostic(invalid, SystemDraftDiagnosticCode::NonPositive,
                           SystemDraftEntityKind::MessageRoute, SystemDraftField::SendOffset));

    draft.set_message_route(duplicate, {.source_task_id = TaskId{8},
                                        .destination_task_id = TaskId{3},
                                        .send_offset = 2,
                                        .delay = 6});
    REQUIRE(draft.build().valid());
    const auto first_json = serialize_experiment_config_json(*draft.build().config);
    const auto second_json = serialize_experiment_config_json(*draft.build().config);
    REQUIRE((first_json == second_json));
}

} // namespace
