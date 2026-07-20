/*** Define detached resource utilization rows for GUI presentation. ***/

#pragma once

#include "cpssim/gui/simulation_controller.hpp"

#include <string>
#include <vector>

namespace cpssim {

struct GuiResourcePresentationRow {
    ResourceId id;
    std::string name;
    Tick busy_ticks;
    Tick idle_ticks;
    double utilization;

    bool operator==(const GuiResourcePresentationRow&) const = default;
};

double calculate_resource_utilization(Tick busy_ticks, Tick idle_ticks) noexcept;
std::vector<GuiResourcePresentationRow>
build_resource_presentation(const SimulationSnapshot& snapshot);

} // namespace cpssim
