/*** Pure generation-based complete-snapshot publication policy. ***/
#pragma once

#include "cpssim/gui/simulation_controller.hpp"

#include <chrono>
#include <cstdint>

namespace cpssim {
struct GuiPresentationGenerations {
    std::uint64_t runtime{};
    std::uint64_t simulation_data{};
    std::uint64_t presentation{};
};
struct GuiPresentationPublicationInput {
    GuiRunMode mode{GuiRunMode::Live};
    GuiControllerUpdateResult update;
    bool switched_fast_to_live{false};
    bool missing_snapshot{false};
    std::uint64_t runtime_generation{};
    std::uint64_t simulation_data_generation{};
    std::chrono::steady_clock::time_point now;
};
class GuiPresentationPublicationPolicy {
  public:
    bool should_publish(const GuiPresentationPublicationInput& input) const noexcept;
    void published(const GuiPresentationPublicationInput& input) noexcept;
    const GuiPresentationGenerations& generations() const noexcept { return generations_; }

  private:
    GuiPresentationGenerations generations_;
    std::chrono::steady_clock::time_point last_publication_{};
};
} // namespace cpssim
