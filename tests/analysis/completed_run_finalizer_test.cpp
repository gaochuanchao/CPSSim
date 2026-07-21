/*** Verify managed completed-run finalizer publication and cancellation. ***/
#include "cpssim/analysis/completed_run_finalizer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <condition_variable>
#include <mutex>

using namespace cpssim;

namespace {
std::shared_ptr<const CompletedRunData> finished_data(std::uint64_t generation) {
    SimulationSnapshot snapshot{.run_state = GuiRunState::Finished,
                                .current_tick = 0,
                                .stop_tick = 0,
                                .experiment = {},
                                .event_log = {},
                                .functional_model_attached = false,
                                .functional_signal_registry = {},
                                .functional_observations = {},
                                .resources = {}};
    return std::make_shared<const CompletedRunData>(
        CompletedRunData{generation, 1, std::move(snapshot)});
}
} // namespace

TEST_CASE("completed finalizer publishes exactly at consumer boundary", "[analysis][finalizer]") {
    std::mutex mutex;
    std::condition_variable ready;
    bool woke = false;
    CompletedRunFinalizer finalizer{[](const auto& request, std::stop_token) {
        auto result = std::make_shared<const RunResult>(
            build_run_result(request.data, request.scenario_kind));
        return CompletedRunResult{
            request.data->runtime_generation, std::move(result), nullptr, request.performance, {}};
    }};
    finalizer.set_wakeup([&] {
        std::lock_guard lock{mutex};
        woke = true;
        ready.notify_one();
    });
    REQUIRE(finalizer.request({finished_data(3), "generic", {}}));
    {
        std::unique_lock lock{mutex};
        REQUIRE(ready.wait_for(lock, std::chrono::seconds{2}, [&] { return woke; }));
    }
    REQUIRE(finalizer.state() == CompletedResultFinalizationState::Finalizing);
    const auto published = finalizer.take_publication();
    REQUIRE(published.has_value());
    REQUIRE(published->run_generation == 3);
    REQUIRE(finalizer.state() == CompletedResultFinalizationState::Ready);
}

TEST_CASE("completed finalizer rejects non-finished data and cancels safely",
          "[analysis][finalizer]") {
    CompletedRunFinalizer finalizer{[](const auto& request, std::stop_token) {
        auto result = std::make_shared<const RunResult>(
            build_run_result(request.data, request.scenario_kind));
        return CompletedRunResult{
            request.data->runtime_generation, std::move(result), nullptr, request.performance, {}};
    }};
    auto data = finished_data(1);
    auto unfinished = std::make_shared<CompletedRunData>(*data);
    unfinished->snapshot.run_state = GuiRunState::Paused;
    REQUIRE_FALSE(finalizer.request({unfinished, "generic", {}}));
    finalizer.cancel();
    REQUIRE(finalizer.state() != CompletedResultFinalizationState::Finalizing);
}
