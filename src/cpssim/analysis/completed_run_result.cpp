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
    auto result =
        std::make_shared<const RunResult>(build_run_result(snapshot, std::move(scenario_kind)));
    completed_ = CompletedRunResult{generation, std::move(result), performance};
    ++build_count_;
    return true;
}

} // namespace cpssim
