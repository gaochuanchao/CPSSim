/*** Verify atomic system rebuild, transition decisions, and persisted applied state. ***/

#include "cpssim/application/project/project_template.hpp"
#include "cpssim/application/project/system_builder_workflow.hpp"
#include "cpssim/config/json_config.hpp"
#include "cpssim/functional/mock_functional_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace cpssim;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        const auto suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
            std::to_string(sequence.fetch_add(1));
        root_ = std::filesystem::temp_directory_path() / ("cpssim-system-builder-" + suffix);
        if (!std::filesystem::create_directory(root_)) {
            throw std::runtime_error{"could not create system-builder test directory"};
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
};

std::unique_ptr<ProjectContext> make_project(const std::filesystem::path& parent, std::string name,
                                             ProjectRuntimeInputs runtime = {}) {
    return create_project(make_generic_project_template(parent, std::move(name)),
                          std::move(runtime));
}

TEST_CASE("valid system apply atomically rebuilds a clean paused project session",
          "[project][system-builder][atomic]") {
    TemporaryDirectory temporary;
    GuiApplicationState state{make_project(temporary.root(), "atomic")};
    const auto* previous_session = &state.active_session();
    EditableSystemDraft draft{state.active_session().config()};
    draft.set_resource_name(0, "renamed CPU");
    draft.set_task_timing(0, {.period = 120, .deadline = 100, .offset = 2}, 3);

    const auto result = apply_system_project_draft(state, draft);

    REQUIRE(result.valid());
    REQUIRE((&state.active_session() != previous_session));
    REQUIRE((state.active_session().config().resources()[0].name() == "renamed CPU"));
    REQUIRE((state.active_session().config().tasks()[0].period() == 120));
    REQUIRE((state.active_session().snapshot().run_state == GuiRunState::Paused));
    REQUIRE(state.active_session().snapshot().event_log.empty());
}

TEST_CASE("invalid config and incompatible run plan preserve the active project",
          "[project][system-builder][atomic]") {
    TemporaryDirectory temporary;
    GuiApplicationState state{make_project(temporary.root(), "retained")};
    const auto* retained = &state.active_session();

    EditableSystemDraft invalid{state.active_session().config()};
    invalid.set_tick_period_ns(0);
    auto invalid_result = apply_system_project_draft(state, invalid);
    REQUIRE_FALSE(invalid_result.valid());
    REQUIRE_FALSE(invalid_result.system_diagnostics.empty());
    REQUIRE((&state.active_session() == retained));

    EditableSystemDraft incompatible{state.active_session().config()};
    const auto added_task = incompatible.add_task();
    incompatible.set_execution_profile(added_task, ResourceId{1}, 1);
    auto incompatible_result = apply_system_project_draft(state, incompatible);
    REQUIRE_FALSE(incompatible_result.valid());
    REQUIRE_FALSE(incompatible_result.run_plan_diagnostics.empty());
    REQUIRE((&state.active_session() == retained));
}

TEST_CASE("explicit builder assignments make a newly added task applicable",
          "[project][system-builder][new-system]") {
    TemporaryDirectory temporary;
    GuiApplicationState state{make_project(temporary.root(), "new-system")};
    EditableSystemDraft draft{state.active_session().config()};
    const auto added_task = draft.add_task();
    draft.set_task_name(1, "second task");
    draft.set_execution_profile(added_task, ResourceId{1}, 2);
    const std::vector<DraftTaskAssignment> assignments{
        {.task_id = TaskId{1}, .resource_id = ResourceId{1}},
        {.task_id = added_task, .resource_id = ResourceId{1}}};

    const auto result = apply_system_project_draft(state, draft, &assignments);
    REQUIRE(result.valid());
    REQUIRE((state.active_session().config().tasks().size() == 2));
    REQUIRE((state.active_session().active_plan()->assignments().size() == 2));
    REQUIRE((state.active_session().snapshot().run_state == GuiRunState::Paused));
}

TEST_CASE("functional model construction failure preserves the active session",
          "[project][system-builder][functional][atomic]") {
    TemporaryDirectory temporary;
    auto calls = std::make_shared<int>(0);
    ProjectRuntimeInputs runtime{
        .functional_model_factory = [calls]() -> std::unique_ptr<FunctionalModel> {
            ++*calls;
            if (*calls > 1) {
                throw std::runtime_error{"injected functional construction failure"};
            }
            return std::make_unique<MockFunctionalModel>();
        },
        .signal_registry = {}};
    GuiApplicationState state{make_project(temporary.root(), "functional", std::move(runtime))};
    const auto* retained = &state.active_session();
    EditableSystemDraft draft{state.active_session().config()};
    draft.set_resource_name(0, "edited");

    const auto result = apply_system_project_draft(state, draft);
    REQUIRE_FALSE(result.valid());
    REQUIRE((result.diagnostic.find("functional") != std::string::npos));
    REQUIRE((&state.active_session() == retained));
}

TEST_CASE("unapplied-change decisions apply and save, discard, or cancel explicitly",
          "[project][system-builder][transition]") {
    TemporaryDirectory temporary;
    GuiApplicationState state{make_project(temporary.root(), "decisions")};
    EditableSystemDraft draft{state.active_session().config()};
    draft.set_resource_name(0, "saved edit");
    const auto* original = &state.active_session();

    auto cancelled =
        resolve_unapplied_system_changes(state, &draft, UnappliedSystemDecision::Cancel);
    REQUIRE((cancelled.status == ProjectTransitionStatus::Cancelled));
    REQUIRE((&state.active_session() == original));

    auto discarded =
        resolve_unapplied_system_changes(state, &draft, UnappliedSystemDecision::Discard);
    REQUIRE((discarded.status == ProjectTransitionStatus::Proceed));
    REQUIRE((&state.active_session() == original));

    auto applied =
        resolve_unapplied_system_changes(state, &draft, UnappliedSystemDecision::ApplyAndSave);
    REQUIRE((applied.status == ProjectTransitionStatus::Proceed));
    REQUIRE((&state.active_session() != original));
    const auto reopened = load_project(state.active_project().root() / "project.json");
    REQUIRE((reopened->session().config().resources()[0].name() == "saved edit"));
    REQUIRE(reopened->session().snapshot().event_log.empty());
}

TEST_CASE("Save Project persists only the applied system and reopening preserves it",
          "[project][system-builder][save]") {
    TemporaryDirectory temporary;
    GuiApplicationState state{make_project(temporary.root(), "save-reopen")};
    EditableSystemDraft draft{state.active_session().config()};
    draft.set_preemption_mode(PreemptionMode::NonPreemptive);
    draft.set_task_name(0, "persisted task");
    REQUIRE(apply_system_project_draft(state, draft).valid());
    save_project(state.active_project());

    const auto system_file = state.active_project().root() / "system.json";
    const auto loaded_system = load_experiment_config(system_file);
    REQUIRE((loaded_system.scheduling().preemption_mode == PreemptionMode::NonPreemptive));
    REQUIRE((loaded_system.tasks()[0].name() == "persisted task"));

    EditableSystemDraft unapplied{state.active_session().config()};
    unapplied.set_task_name(0, "not persisted");
    save_project(state.active_project());
    const auto reopened = load_project(state.active_project().root() / "project.json");
    REQUIRE((reopened->session().config().tasks()[0].name() == "persisted task"));
}

} // namespace
