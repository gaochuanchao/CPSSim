/*** Build final analysis exactly once after a generation reaches Finished. ***/

#include "cpssim/analysis/completed_run_result.hpp"

namespace cpssim {

bool CompletedRunResultCache::publish_finished(std::uint64_t generation,
                                               const SimulationSnapshot& snapshot,
                                               std::string scenario_kind,
                                               RunPerformanceSummary performance) {
    if (snapshot.run_state != GuiRunState::Finished) {
        return false;
    }
    if (completed_.has_value() && completed_->run_generation == generation) {
        return false;
    }
    auto data = std::make_shared<const CompletedRunData>(CompletedRunData{generation, 0, snapshot});
    return publish_finished(std::move(data), std::move(scenario_kind), performance);
}

bool CompletedRunResultCache::publish_finished(
    std::shared_ptr<const CompletedRunData> completed_data, std::string scenario_kind,
    RunPerformanceSummary performance) {
    if (completed_data == nullptr || completed_data->snapshot.run_state != GuiRunState::Finished) {
        return false;
    }
    const auto generation = completed_data->runtime_generation;
    if (completed_.has_value() && completed_->run_generation == generation) {
        return false;
    }
    auto result = std::make_shared<const RunResult>(
        build_run_result(std::move(completed_data), std::move(scenario_kind)));
    completed_ = CompletedRunResult{generation, std::move(result), nullptr, performance, {}};
    ++build_count_;
    return true;
}

bool CompletedRunResultCache::publish_ready(CompletedRunResult completed) {
    if (completed.result == nullptr ||
        completed.result->snapshot().run_state != GuiRunState::Finished ||
        (completed_.has_value() && completed_->run_generation == completed.run_generation)) {
        return false;
    }
    completed_ = std::move(completed);
    ++build_count_;
    return true;
}

} // namespace cpssim
