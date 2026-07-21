/*** Finalize immutable run data off-thread and publish only at GUI boundaries. ***/
#include "cpssim/analysis/completed_run_finalizer.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace cpssim {

CompletedRunFinalizer::CompletedRunFinalizer(CompletedRunFinalizationBuilder builder)
    : builder_{std::move(builder)} {
    if (!builder_) {
        throw std::invalid_argument{"completed-run finalizer requires a builder"};
    }
}

CompletedRunFinalizer::~CompletedRunFinalizer() { cancel(); }

bool CompletedRunFinalizer::request(CompletedRunFinalizationRequest request) {
    if (request.data == nullptr || request.data->snapshot.run_state != GuiRunState::Finished) {
        return false;
    }
    cancel();
    {
        std::lock_guard lock{mutex_};
        if (requested_generation_ == request.data->runtime_generation &&
            state_ == CompletedResultFinalizationState::Ready) {
            return false;
        }
        requested_generation_ = request.data->runtime_generation;
        pending_result_.reset();
        pending_error_.reset();
        state_ = CompletedResultFinalizationState::Finalizing;
    }
    worker_ = std::jthread([this, request = std::move(request)](std::stop_token stop) mutable {
        try {
            auto result = builder_(request, stop);
            if (stop.stop_requested()) {
                return;
            }
            std::function<void()> wakeup;
            {
                std::lock_guard lock{mutex_};
                pending_result_ = std::move(result);
                wakeup = wakeup_;
            }
            if (wakeup) {
                wakeup();
            }
        } catch (const std::exception& error) {
            std::function<void()> wakeup;
            {
                std::lock_guard lock{mutex_};
                pending_error_ = error.what();
                wakeup = wakeup_;
            }
            if (wakeup) {
                wakeup();
            }
        }
    });
    return true;
}

void CompletedRunFinalizer::cancel() {
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
    std::lock_guard lock{mutex_};
    if (state_ == CompletedResultFinalizationState::Finalizing) {
        state_ = CompletedResultFinalizationState::Cancelled;
    }
    pending_result_.reset();
    pending_error_.reset();
}

void CompletedRunFinalizer::reset() {
    cancel();
    std::lock_guard lock{mutex_};
    requested_generation_.reset();
    state_ = CompletedResultFinalizationState::Idle;
}

void CompletedRunFinalizer::set_wakeup(std::function<void()> wakeup) {
    std::lock_guard lock{mutex_};
    wakeup_ = std::move(wakeup);
}

CompletedResultFinalizationState CompletedRunFinalizer::state() const {
    std::lock_guard lock{mutex_};
    return state_;
}

bool CompletedRunFinalizer::publication_pending() const {
    std::lock_guard lock{mutex_};
    return pending_result_.has_value() ||
           (pending_error_.has_value() &&
            state_ == CompletedResultFinalizationState::Finalizing);
}

std::optional<CompletedRunResult> CompletedRunFinalizer::take_publication() {
    std::lock_guard lock{mutex_};
    if (pending_result_.has_value()) {
        auto result = std::move(pending_result_);
        pending_result_.reset();
        state_ = CompletedResultFinalizationState::Ready;
        return result;
    }
    if (pending_error_.has_value()) {
        state_ = CompletedResultFinalizationState::Failed;
    }
    return std::nullopt;
}

std::optional<std::string> CompletedRunFinalizer::diagnostic() const {
    std::lock_guard lock{mutex_};
    return pending_error_;
}

} // namespace cpssim
