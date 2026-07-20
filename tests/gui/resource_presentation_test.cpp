/*** Verify detached resource utilization presentation. ***/

#include "cpssim/gui/resource_presentation.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cpssim;

TEST_CASE("resource utilization handles observed and zero tick totals", "[gui][resources]") {
    REQUIRE(calculate_resource_utilization(3, 1) == 0.75);
    REQUIRE(calculate_resource_utilization(0, 0) == 0.0);
}

TEST_CASE("resource presentation preserves stable selection identities", "[gui][resources]") {
    SimulationSnapshot snapshot{
        .run_state = GuiRunState::Paused,
        .current_tick = 0,
        .stop_tick = 10,
        .experiment = {},
        .event_log = {},
        .functional_model_attached = false,
        .functional_signal_registry = {},
        .functional_observations = {},
        .resources = {{ResourceId{7}, "long resource", std::nullopt, {}, 4, 6}}};
    const auto rows = build_resource_presentation(snapshot);
    REQUIRE(rows.size() == 1);
    REQUIRE(rows.front().id == ResourceId{7});
    REQUIRE(rows.front().name == "long resource");
    REQUIRE(rows.front().utilization == 0.4);
}
