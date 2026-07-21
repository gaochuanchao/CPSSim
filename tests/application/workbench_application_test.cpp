/*** Verify graphics-independent workbench ownership and atomic lifecycle. ***/
#include "cpssim/application/project/project_template.hpp"
#include "cpssim/application/workbench_application.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace {

using namespace cpssim;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        root_ = std::filesystem::temp_directory_path() / ("cpssim-workbench-" + suffix);
        std::filesystem::create_directories(root_);
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(root_); }
    const std::filesystem::path& path() const { return root_; }

  private:
    std::filesystem::path root_;
};

ExperimentConfig make_config(std::string resource_name = "cpu") {
    return ExperimentConfig{std::chrono::nanoseconds{100'000},
                            SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
                            {ResourceSpec{ResourceId{1}, std::move(resource_name)}},
                            {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{10, 10, 0}, 1}},
                            {TaskResourceProfile{TaskId{1}, ResourceId{1}, 1}}};
}

std::unique_ptr<GuiSimulationSession> make_session() {
    auto session = std::make_unique<GuiSimulationSession>(make_config(), 20);
    REQUIRE(session->set_draft_assignment(TaskId{1}, ResourceId{1}));
    REQUIRE(session->apply_draft());
    return session;
}

TEST_CASE("workbench starts on Home and owns frontend-independent selections",
          "[application][workbench]") {
    WorkbenchApplication workbench;
    REQUIRE(workbench.screen() == GuiApplicationScreen::Home);
    REQUIRE(workbench.run_state() == GuiRunState::NotConfigured);
    REQUIRE(workbench.structural_selection().kind() == StructuralSelectionKind::System);
    REQUIRE(workbench.presentation_snapshot() == nullptr);
}

TEST_CASE("workbench session replacement publishes one detached paused snapshot",
          "[application][workbench]") {
    WorkbenchApplication workbench{make_session()};
    REQUIRE(workbench.screen() == GuiApplicationScreen::Workbench);
    REQUIRE(workbench.run_state() == GuiRunState::Paused);
    const auto snapshot = workbench.presentation_snapshot();
    REQUIRE(snapshot != nullptr);
    REQUIRE(snapshot->run_state == GuiRunState::Paused);

    workbench.close_project();
    REQUIRE(workbench.screen() == GuiApplicationScreen::Home);
    REQUIRE(snapshot->run_state == GuiRunState::Paused);
}

TEST_CASE("workbench applies valid draft atomically and owns diagnostics",
          "[application][workbench]") {
    TemporaryDirectory temporary;
    auto request = make_generic_project_template(temporary.path(), "project");
    WorkbenchApplication workbench;
    workbench.create_project(std::move(request));
    REQUIRE(workbench.has_active_project());
    REQUIRE(workbench.editable_system().has_value());
    workbench.editable_system()->set_task_name(0, "renamed");
    REQUIRE(workbench.system_changes_dirty());
    REQUIRE(workbench.apply_system_draft());
    REQUIRE(workbench.active_session().config().tasks().front().name() == "renamed");
    REQUIRE_FALSE(workbench.status_is_error());
}

TEST_CASE("failed project loading does not replace workbench ownership",
          "[application][workbench][atomic]") {
    TemporaryDirectory temporary;
    WorkbenchApplication workbench;
    workbench.create_project(make_generic_project_template(temporary.path(), "retained"));
    const auto retained_root = workbench.active_project().root();

    REQUIRE_THROWS(workbench.open_project(temporary.path() / "missing" / "project.json"));
    REQUIRE(workbench.active_project().root() == retained_root);
    REQUIRE(workbench.run_state() == GuiRunState::Paused);
}

TEST_CASE("workbench finalizes one completed generation outside toolkit code",
          "[application][workbench][result]") {
    WorkbenchApplication workbench{make_session()};
    REQUIRE(workbench.enqueue(GuiCommand::Run));
    while (workbench.run_state() != GuiRunState::Finished) {
        static_cast<void>(workbench.update());
    }
    REQUIRE(workbench.finalization_state() == CompletedResultFinalizationState::Finalizing);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!workbench.process_background_publications() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    REQUIRE(workbench.completed_result() != nullptr);
    REQUIRE(workbench.completed_result()->run_generation ==
            workbench.active_session().runtime_generation());
}

} // namespace
