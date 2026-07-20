/*** Own one immutable finish-only analysis result per runtime generation. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace cpssim {

struct CompletedRunResult {
    std::uint64_t run_generation{};
    std::shared_ptr<const RunResult> result;
    RunPerformanceSummary performance;
};

class CompletedRunResultCache {
  public:
    bool publish_finished(std::uint64_t generation, const SimulationSnapshot& snapshot,
                          std::string scenario_kind, RunPerformanceSummary performance);
    void invalidate() noexcept { completed_.reset(); }
    const CompletedRunResult* get() const noexcept { return completed_ ? &*completed_ : nullptr; }
    std::size_t build_count() const noexcept { return build_count_; }

  private:
    std::optional<CompletedRunResult> completed_;
    std::size_t build_count_{};
};

} // namespace cpssim
