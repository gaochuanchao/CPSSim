/***
 * File: tests/gui/application_state_test.cpp
 * Purpose: Verify optional GUI-session ownership and Home/Workbench state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#include "cpssim/gui/application_state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using namespace cpssim;

ExperimentConfig make_application_config(std::string resource_name) {
    return ExperimentConfig{
        std::chrono::nanoseconds{100'000},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, std::move(resource_name)}},
        {TaskSpec{TaskId{1}, "task", PeriodicTimingSpec{.period = 10, .deadline = 10, .offset = 0},
                  1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 1}}};
}

std::unique_ptr<GuiSimulationSession> make_session(std::string resource_name) {
    return std::make_unique<GuiSimulationSession>(make_application_config(std::move(resource_name)),
                                                  100);
}

TEST_CASE("GUI application state starts at Home without constructing a session",
          "[gui][application][home]") {
    GuiApplicationState application_state;

    REQUIRE((application_state.screen() == GuiApplicationScreen::Home));
    REQUIRE_FALSE(application_state.has_active_session());
    REQUIRE_THROWS_AS(application_state.active_session(), std::logic_error);
}

TEST_CASE("supplying a GUI session selects the Workbench", "[gui][application][workbench]") {
    GuiApplicationState application_state{make_session("initial")};

    REQUIRE((application_state.screen() == GuiApplicationScreen::Workbench));
    REQUIRE(application_state.has_active_session());
    REQUIRE((application_state.active_session().config().resources()[0].name() == "initial"));
}

TEST_CASE("GUI session replacement and clearing update ownership safely",
          "[gui][application][ownership]") {
    GuiApplicationState application_state{make_session("first")};
    auto replacement = make_session("replacement");
    const auto* replacement_address = replacement.get();

    application_state.replace_session(std::move(replacement));
    REQUIRE((application_state.screen() == GuiApplicationScreen::Workbench));
    REQUIRE((&application_state.active_session() == replacement_address));
    REQUIRE((application_state.active_session().config().resources()[0].name() == "replacement"));

    application_state.clear_session();
    REQUIRE((application_state.screen() == GuiApplicationScreen::Home));
    REQUIRE_FALSE(application_state.has_active_session());
    REQUIRE_THROWS_AS(application_state.active_session(), std::logic_error);
}

TEST_CASE("GUI application state rejects an empty replacement", "[gui][application][ownership]") {
    GuiApplicationState application_state{make_session("retained")};

    REQUIRE_THROWS_AS(application_state.replace_session(nullptr), std::invalid_argument);
    REQUIRE((application_state.screen() == GuiApplicationScreen::Workbench));
    REQUIRE((application_state.active_session().config().resources()[0].name() == "retained"));
}

} // namespace
