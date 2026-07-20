/*** Verify typed Bosch plot-series derivation and missing-signal behavior. ***/

#include "cpssim/application/bosch_result_analysis.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace cpssim;

RunResult bosch_result(bool include_lateral = true) {
    SimulationSnapshot snapshot{.run_state = GuiRunState::Finished,
                                .current_tick = 4,
                                .stop_tick = 4,
                                .experiment = {},
                                .event_log = {},
                                .functional_model_attached = true,
                                .functional_signal_registry = {},
                                .functional_observations = {},
                                .resources = {}};
    if (include_lateral) {
        snapshot.functional_signal_registry.push_back({.id = bosch_lateral_error_signal_id(),
                                                       .path = "Bosch/Control/Lateral error",
                                                       .display_name = "Lateral error",
                                                       .unit = "m",
                                                       .source = "Bosch FMU"});
    }
    snapshot.functional_signal_registry.push_back({.id = bosch_critical_section_signal_id(),
                                                   .path = "Bosch/Timing/Critical section",
                                                   .display_name = "Critical section",
                                                   .unit = "",
                                                   .source = "Bosch FMU"});
    for (Tick tick = 0; tick <= 4; ++tick) {
        FunctionalObservation observation{
            .tick = tick,
            .real_signals = {},
            .integer_signals = {},
            .boolean_signals = {
                {.name = "critical_section", .value = tick == 1 || tick == 2 || tick == 4}}};
        if (include_lateral) {
            constexpr double values[]{0.0, 0.25, 0.1, -0.25, -0.1};
            observation.real_signals.push_back(
                {.name = "lateral_error", .value = values[static_cast<std::size_t>(tick)]});
        }
        snapshot.functional_observations.push_back(std::move(observation));
    }
    EventEntityRefs refs;
    refs.task_id = TaskId{1};
    snapshot.event_log.emplace_back(3, EventPhase::DeadlineCheck, EventSequence{0},
                                    EventType::DeadlineMiss, refs);
    return build_run_result(std::move(snapshot), "bosch");
}

TEST_CASE("Bosch analysis maps typed signals, thresholds, intervals, and deadline markers",
          "[project][result][bosch]") {
    const auto result = bosch_result();
    const auto analysis = derive_bosch_result_analysis(result);
    REQUIRE(analysis.lateral_error != nullptr);
    REQUIRE(analysis.lateral_error->descriptor.unit == "m");
    REQUIRE(analysis.critical_intervals == std::vector<BoschCriticalInterval>{{1, 2}, {4, 4}});
    REQUIRE(analysis.threshold_crossings ==
            std::vector<BoschThresholdCrossing>{{3, -0.2}, {4, -0.2}, {1, 0.2}, {2, 0.2}});
    REQUIRE(analysis.deadline_miss_ticks == std::vector<Tick>{3});
}

TEST_CASE("Bosch analysis reports unavailable required signals", "[project][result][bosch]") {
    const auto result = bosch_result(false);
    const auto analysis = derive_bosch_result_analysis(result);
    REQUIRE(analysis.lateral_error == nullptr);
    REQUIRE(analysis.diagnostic.has_value());
}

} // namespace
