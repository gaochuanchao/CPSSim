/*** Implement resource presentation derivation from detached snapshots. ***/

#include "cpssim/gui/resource_presentation.hpp"

namespace cpssim {

double calculate_resource_utilization(Tick busy_ticks, Tick idle_ticks) noexcept {
    const auto observed = busy_ticks + idle_ticks;
    return observed > 0 ? static_cast<double>(busy_ticks) / static_cast<double>(observed) : 0.0;
}

std::vector<GuiResourcePresentationRow>
build_resource_presentation(const SimulationSnapshot& snapshot) {
    std::vector<GuiResourcePresentationRow> rows;
    rows.reserve(snapshot.resources.size());
    for (const auto& resource : snapshot.resources) {
        rows.push_back({.id = resource.id,
                        .name = resource.name,
                        .busy_ticks = resource.busy_ticks,
                        .idle_ticks = resource.idle_ticks,
                        .utilization = calculate_resource_utilization(resource.busy_ticks,
                                                                      resource.idle_ticks)});
    }
    return rows;
}

} // namespace cpssim
