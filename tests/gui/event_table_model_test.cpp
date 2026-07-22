/*** Verify canonical event projection, filtering, ordering, and cause navigation. ***/

#include "cpssim/gui/event_table_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace cpssim;

namespace {
SimulationSnapshot event_snapshot() {
    return {.run_state = GuiRunState::Paused,
            .current_tick = 2,
            .stop_tick = 10,
            .experiment = {.tick_period = std::chrono::microseconds{100},
                           .preemption_mode = PreemptionMode::Preemptive,
                           .resources = {},
                           .tasks = {},
                           .profiles = {},
                           .routes = {},
                           .assignments = {}},
            .event_log = {Event{2,
                                EventPhase::ExecutionCompletion,
                                EventSequence{2},
                                EventType::JobFinish,
                                {.task_id = TaskId{1},
                                 .job_id = JobId{3},
                                 .resource_id = ResourceId{4},
                                 .message_id = std::nullopt,
                                 .vehicle_id = VehicleId{5}},
                                EventSequence{1}},
                          Event{0,
                                EventPhase::JobRelease,
                                EventSequence{1},
                                EventType::JobRelease,
                                {.task_id = TaskId{1},
                                 .job_id = JobId{3},
                                 .resource_id = std::nullopt,
                                 .message_id = std::nullopt,
                                 .vehicle_id = std::nullopt}}},
            .functional_model_attached = false,
            .functional_signal_registry = {},
            .functional_observations = {},
            .resources = {}};
}
} // namespace

TEST_CASE("event rows preserve canonical source order with missing IDs", "[gui][events]") {
    const auto rows = build_event_table_rows(event_snapshot());
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].sequence == EventSequence{2});
    REQUIRE(rows[1].sequence == EventSequence{1});
    REQUIRE_FALSE(rows[1].entities.resource_id.has_value());
    REQUIRE(rows[0].time_milliseconds == 0.2);
    REQUIRE(find_event_row_by_sequence(rows, *rows[0].cause) == 1);
}

TEST_CASE("event filters project indices without mutating canonical rows", "[gui][events]") {
    const auto rows = build_event_table_rows(event_snapshot());
    GuiEventFilters filters;
    filters.type = EventType::JobFinish;
    filters.resource = ResourceId{4};
    filters.vehicle = VehicleId{5};
    filters.text = "execution completion";
    const auto projected = filter_event_table_rows(rows, filters);
    REQUIRE(projected == std::vector<std::size_t>{0});
    REQUIRE(rows[0].sequence == EventSequence{2});
}

TEST_CASE("event cache keys rows by presentation generation and debounces text",
          "[gui][events][cache]") {
    GuiEventTableCache cache;
    auto source = event_snapshot();
    const auto start = std::chrono::steady_clock::now();
    REQUIRE(cache.update_rows(1, source));
    REQUIRE_FALSE(cache.update_rows(1, source));
    REQUIRE(cache.row_build_count() == 1);
    GuiEventFilters filters;
    REQUIRE(cache.update_filter(filters, start));
    filters.text = "deadline";
    REQUIRE_FALSE(cache.update_filter(filters, start + std::chrono::milliseconds{100}));
    REQUIRE(cache.update_filter(filters, start + std::chrono::milliseconds{251}));
    REQUIRE(cache.filtered_indices().empty());
    REQUIRE_FALSE(event_raw_json(source, EventSequence{999}).size() > 0);
    REQUIRE(event_raw_json(source, EventSequence{2}).find("job_finish") != std::string::npos);
}

TEST_CASE("large event projection retains canonical order", "[gui][events][large]") {
    auto snapshot = event_snapshot();
    snapshot.event_log.clear();
    for (std::uint64_t sequence = 1; sequence <= 100'000; ++sequence) {
        snapshot.event_log.emplace_back(static_cast<Tick>(sequence), EventPhase::JobRelease,
                                        EventSequence{sequence}, EventType::JobRelease);
    }
    const auto rows = build_event_table_rows(snapshot);
    const auto projected = filter_event_table_rows(rows, {});
    REQUIRE(projected.size() == 100'000);
    REQUIRE(rows[projected.back()].sequence == EventSequence{100'000});
}
