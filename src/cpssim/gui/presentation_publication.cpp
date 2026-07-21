/*** Implement coherent-boundary and rate-limited Live snapshot publication. ***/
#include "cpssim/gui/presentation_publication.hpp"

namespace cpssim {
bool GuiPresentationPublicationPolicy::should_publish(
    const GuiPresentationPublicationInput& input) const noexcept {
    if (input.missing_snapshot || input.runtime_generation != generations_.runtime ||
        input.update.reset || input.update.paused || input.update.finished ||
        input.update.step_completed || input.switched_fast_to_live) {
        return true;
    }
    if (input.simulation_data_generation == generations_.simulation_data ||
        input.mode != GuiRunMode::Live) {
        return false;
    }
    constexpr auto maximum_period = std::chrono::microseconds{66'667};
    return last_publication_ == std::chrono::steady_clock::time_point{} ||
           input.now - last_publication_ >= maximum_period;
}
void GuiPresentationPublicationPolicy::published(
    const GuiPresentationPublicationInput& input) noexcept {
    generations_.runtime = input.runtime_generation;
    generations_.simulation_data = input.simulation_data_generation;
    ++generations_.presentation;
    last_publication_ = input.now;
}
} // namespace cpssim
