/***
 * File: tests/gui/signal_series_test.cpp
 * Purpose: Verify G06 typed extraction, live caching, and visual downsampling.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/signal_series.hpp"
#include "cpssim/gui/plot_visualizer_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace cpssim;

FunctionalObservation observation(Tick tick, double state, std::int64_t count, bool active) {
    return {.tick = tick,
            .real_signals = {{.name = "state", .value = state}},
            .integer_signals = {{.name = "count", .value = count}},
            .boolean_signals = {{.name = "active", .value = active}}};
}

const GuiSignalSeries* find_series(const GuiSignalModel& model, GuiSignalScalarType type,
                                   const std::string& name) {
    return find_signal_series(model, GuiSignalId{type, name});
}

TEST_CASE("signal model handles missing observations and discovers typed stable identities",
          "[gui][signal][model]") {
    const auto empty = build_signal_model({});
    REQUIRE(empty.valid());
    REQUIRE(empty.model->series.empty());

    const std::vector<FunctionalObservation> observations{observation(0, 1.5, 2, false),
                                                          observation(1, -3.0, 4, true)};
    const auto result = build_signal_model(observations);
    REQUIRE(result.valid());
    REQUIRE((result.model->series.size() == 3));

    const auto* real = find_series(*result.model, GuiSignalScalarType::Real, "state");
    const auto* integer = find_series(*result.model, GuiSignalScalarType::Integer, "count");
    const auto* boolean = find_series(*result.model, GuiSignalScalarType::Boolean, "active");
    REQUIRE(real != nullptr);
    REQUIRE(integer != nullptr);
    REQUIRE(boolean != nullptr);
    REQUIRE((real->descriptor.path == "Functional/Real/state"));
    REQUIRE(real->descriptor.unit.empty());
    REQUIRE((std::get<double>(real->samples[1].value) == -3.0));
    REQUIRE((std::get<std::int64_t>(integer->samples[1].value) == 4));
    REQUIRE(std::get<bool>(boolean->samples[1].value));
    REQUIRE((real->samples[1].tick == 1));
}

TEST_CASE("signal registry supplies presentation names and units without changing identity",
          "[gui][signal][registry]") {
    const std::vector<GuiSignalDescriptor> registry{{.id = {GuiSignalScalarType::Real, "state"},
                                                     .path = "Functional/Mock/State",
                                                     .display_name = "Mock state",
                                                     .unit = "m",
                                                     .source = "mock-adapter"},
                                                    {.id = {GuiSignalScalarType::Integer, "count"},
                                                     .path = "Functional/Mock/Count",
                                                     .display_name = "Action count",
                                                     .unit = "events",
                                                     .source = "mock-adapter"},
                                                    {.id = {GuiSignalScalarType::Boolean, "active"},
                                                     .path = "Functional/Mock/Active",
                                                     .display_name = "Active",
                                                     .unit = "",
                                                     .source = "mock-adapter"}};
    const auto result = build_signal_model({observation(0, 1.0, 2, true)}, registry);

    REQUIRE(result.valid());
    const auto* state = find_series(*result.model, GuiSignalScalarType::Real, "state");
    REQUIRE(state != nullptr);
    REQUIRE((state->descriptor.display_name == "Mock state"));
    REQUIRE((state->descriptor.unit == "m"));
    REQUIRE((state->descriptor.source == "mock-adapter"));
    REQUIRE((state->descriptor.id == GuiSignalId{GuiSignalScalarType::Real, "state"}));
}

TEST_CASE("signal diagnostics locate tick and schema errors", "[gui][signal][invalid]") {
    SECTION("unexpected tick") {
        const auto result = build_signal_model({observation(1, 0.0, 0, false)});
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiSignalDiagnosticCode::InvalidObservationTick));
        REQUIRE((result.diagnostics[0].observation_index == 0));
        REQUIRE((result.diagnostics[0].tick == 1));
        REQUIRE(
            (result.diagnostics[0].message.find("observation[0] at tick 1") != std::string::npos));
    }

    SECTION("changed schema") {
        auto changed = observation(1, 0.0, 0, false);
        changed.real_signals[0].name = "other";
        const auto result = build_signal_model({observation(0, 0.0, 0, false), changed});
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiSignalDiagnosticCode::SchemaMismatch));
        REQUIRE((result.diagnostics[0].observation_index == 1));
        REQUIRE(
            (result.diagnostics[0].signal_id == GuiSignalId{GuiSignalScalarType::Real, "other"}));
    }

    SECTION("registry mismatch") {
        const std::vector<GuiSignalDescriptor> incomplete{
            {.id = {GuiSignalScalarType::Real, "state"},
             .path = "state",
             .display_name = "State",
             .unit = "",
             .source = "test"}};
        const auto result = build_signal_model({observation(0, 0.0, 0, false)}, incomplete);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiSignalDiagnosticCode::SchemaMismatch));
        REQUIRE((result.diagnostics[0].signal_id ==
                 GuiSignalId{GuiSignalScalarType::Integer, "count"}));
        REQUIRE(
            (result.diagnostics[0].message.find("observation[0] at tick 0") != std::string::npos));
    }

    SECTION("invalid registry field") {
        const std::vector<GuiSignalDescriptor> invalid{{.id = {GuiSignalScalarType::Real, "state"},
                                                        .path = "",
                                                        .display_name = "State",
                                                        .unit = "",
                                                        .source = "test"}};
        const auto result = build_signal_model({}, invalid);
        REQUIRE_FALSE(result.valid());
        REQUIRE((result.diagnostics[0].code == GuiSignalDiagnosticCode::InvalidRegistry));
        REQUIRE((result.diagnostics[0].observation_index == 0));
        REQUIRE((result.diagnostics[0].message.find("signal registry[0]") != std::string::npos));
        REQUIRE(
            (result.diagnostics[0].message.find("path must not be empty") != std::string::npos));
    }
}

TEST_CASE("signal cache incrementally equals complete rebuild and clears on reset",
          "[gui][signal][cache]") {
    const std::vector<FunctionalObservation> observations{observation(0, 1.0, 0, false),
                                                          observation(1, 3.0, 1, true),
                                                          observation(2, -2.0, 1, false)};
    GuiSignalCache cache;
    for (std::size_t count = 1; count <= observations.size(); ++count) {
        const std::vector<FunctionalObservation> prefix{
            observations.begin(), observations.begin() + static_cast<std::ptrdiff_t>(count)};
        const auto rebuilt = build_signal_model(prefix);
        const auto& incremental = cache.update(prefix);
        REQUIRE(incremental.valid());
        REQUIRE((incremental.model == rebuilt.model));
    }

    auto invalid_suffix = observation(3, 4.0, 2, true);
    invalid_suffix.real_signals[0].name = "changed";
    auto invalid_trace = observations;
    invalid_trace.push_back(invalid_suffix);
    const auto& invalid = cache.update(invalid_trace);
    REQUIRE_FALSE(invalid.valid());
    REQUIRE((invalid.diagnostics[0].observation_index == 3));
    const auto& recovered = cache.update(observations);
    REQUIRE(recovered.valid());
    REQUIRE((recovered.model == build_signal_model(observations).model));

    const auto& reset = cache.update({});
    REQUIRE(reset.valid());
    REQUIRE(reset.model->series.empty());
}

TEST_CASE("signal downsampling is deterministic and preserves visible extrema",
          "[gui][signal][downsample]") {
    GuiSignalSeries series{.descriptor = {.id = {GuiSignalScalarType::Real, "spike"},
                                          .path = "Functional/Real/spike",
                                          .display_name = "Spike",
                                          .unit = "",
                                          .source = "test"},
                           .samples = {}};
    const std::vector<double> values{0.0, 1.0, 100.0, -50.0, 3.0, 4.0, 5.0, 6.0, 7.0, 9.0};
    for (std::size_t index = 0; index < values.size(); ++index) {
        series.samples.push_back({.tick = static_cast<Tick>(index), .value = values[index]});
    }

    const auto first =
        downsample_signal(series, {.begin_tick = 0, .end_tick = 9, .maximum_points = 4});
    const auto second =
        downsample_signal(series, {.begin_tick = 0, .end_tick = 9, .maximum_points = 4});
    REQUIRE((first == second));
    REQUIRE((first.size() == 4));
    REQUIRE((first.front() == series.samples.front()));
    REQUIRE((first.back() == series.samples.back()));
    REQUIRE(std::any_of(first.begin(), first.end(), [](const GuiScalarSample& sample) {
        return gui_scalar_as_double(sample.value) == 100.0;
    }));
    REQUIRE(std::any_of(first.begin(), first.end(), [](const GuiScalarSample& sample) {
        return gui_scalar_as_double(sample.value) == -50.0;
    }));

    const auto visible =
        downsample_signal(series, {.begin_tick = 1, .end_tick = 8, .maximum_points = 4});
    REQUIRE((visible.front().tick == 1));
    REQUIRE((visible.back().tick == 8));
    REQUIRE((series.samples.size() == values.size()));
}

TEST_CASE("completed plot browser searches and groups selected signals by unit",
          "[gui][signal][visualizer]") {
    const std::vector<GuiSignalDescriptor> registry{
        {{GuiSignalScalarType::Real, "state"}, "Plant/State", "State", "m", "test"},
        {{GuiSignalScalarType::Integer, "count"}, "Plant/Count", "Count", "m", "test"},
        {{GuiSignalScalarType::Boolean, "active"}, "Plant/Active", "Active", "", "test"}};
    const auto built = build_signal_model({observation(0, 1.0, 2, true)}, registry);
    REQUIRE(search_plot_signals(*built.model, "state").size() == 1);
    const auto lanes = build_plot_lanes(*built.model,
                                        {{GuiSignalScalarType::Real, "state"},
                                         {GuiSignalScalarType::Integer, "count"},
                                         {GuiSignalScalarType::Boolean, "active"}});
    REQUIRE(lanes.size() == 2);
    REQUIRE(lanes[0].unit == "m");
    REQUIRE(lanes[0].series.size() == 2);
    REQUIRE(lanes[1].digital);
}

TEST_CASE("plot ranges and tick units retain exact source ticks",
          "[gui][signal][visualizer][range]") {
    auto value = SimulationSnapshot{.run_state = GuiRunState::Finished,
                                    .current_tick = 20,
                                    .stop_tick = 20,
                                    .experiment = {}, .event_log = {},
                                    .functional_model_attached = false,
                                    .functional_signal_registry = {},
                                    .functional_observations = {}, .resources = {}};
    value.experiment.tick_period = std::chrono::milliseconds{2};
    const auto result = build_run_result(std::move(value), "generic");
    REQUIRE(resolve_plot_range(result, GuiPlotRangeMode::Selected, GuiTickRange{3, 7}, 0, 0) == GuiPlotRange{3, 7});
    REQUIRE(resolve_plot_range(result, GuiPlotRangeMode::Custom, std::nullopt, 4, 99) == GuiPlotRange{4, 20});
    REQUIRE(plot_tick_coordinate(5, GuiPlotXAxisUnit::Seconds, result.metrics.tick_period) == 0.01);
}

} // namespace
