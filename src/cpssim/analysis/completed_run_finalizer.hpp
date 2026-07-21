/*** Managed background finalization of immutable completed-run data. ***/
#pragma once

#include "cpssim/analysis/completed_run_result.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace cpssim {

enum class CompletedResultFinalizationState { Idle, Finalizing, Ready, Cancelled, Failed };

struct CompletedRunFinalizationRequest {
    std::shared_ptr<const CompletedRunData> data;
    std::string scenario_kind;
    RunPerformanceSummary performance;
};

using CompletedRunFinalizationBuilder =
    std::function<CompletedRunResult(const CompletedRunFinalizationRequest&, std::stop_token)>;

class CompletedRunFinalizer {
  public:
    explicit CompletedRunFinalizer(CompletedRunFinalizationBuilder builder);
    ~CompletedRunFinalizer();
    CompletedRunFinalizer(const CompletedRunFinalizer&) = delete;
    CompletedRunFinalizer& operator=(const CompletedRunFinalizer&) = delete;

    bool request(CompletedRunFinalizationRequest request);
    void cancel();
    void reset();
    void set_wakeup(std::function<void()> wakeup);
    CompletedResultFinalizationState state() const;
    bool publication_pending() const;
    std::optional<CompletedRunResult> take_publication();
    std::optional<std::string> diagnostic() const;

  private:
    CompletedRunFinalizationBuilder builder_;
    mutable std::mutex mutex_;
    std::jthread worker_;
    CompletedResultFinalizationState state_{CompletedResultFinalizationState::Idle};
    std::optional<CompletedRunResult> pending_result_;
    std::optional<std::string> pending_error_;
    std::function<void()> wakeup_;
    std::optional<std::uint64_t> requested_generation_;
};

} // namespace cpssim
