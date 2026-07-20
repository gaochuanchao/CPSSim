/*** Implement presentation-only workspace normalization and splitter geometry. ***/

#include "cpssim/gui/workspace_state.hpp"

#include <algorithm>
#include <cmath>

namespace cpssim {

float normalize_splitter_ratio(float ratio, float fallback) noexcept {
    const auto safe_fallback = std::isfinite(fallback) ? std::clamp(fallback, 0.05F, 0.95F) : 0.5F;
    return std::isfinite(ratio) ? std::clamp(ratio, 0.05F, 0.95F) : safe_fallback;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- named declaration documents layout units.
GuiVerticalSplit calculate_vertical_split(float available_height, float splitter_height,
                                          float desired_ratio, float minimum_first,
                                          float minimum_second) noexcept {
    const auto available = std::max(0.0F, available_height);
    const auto separator = std::clamp(splitter_height, 0.0F, available);
    const auto usable = std::max(0.0F, available - separator);
    if (usable == 0.0F) {
        return {.first_height = 0.0F, .second_height = 0.0F, .normalized_ratio = 0.5F};
    }
    auto first_minimum = std::clamp(minimum_first, 0.0F, usable);
    auto second_minimum = std::clamp(minimum_second, 0.0F, usable);
    if (first_minimum + second_minimum > usable) {
        const auto scale = usable / (first_minimum + second_minimum);
        first_minimum *= scale;
        second_minimum *= scale;
    }
    const auto ratio = normalize_splitter_ratio(desired_ratio, 0.5F);
    const auto first = std::clamp(usable * ratio, first_minimum, usable - second_minimum);
    return {
        .first_height = first, .second_height = usable - first, .normalized_ratio = first / usable};
}

void normalize_workspace_state(GuiWorkspaceState& workspace) noexcept {
    workspace.schema_version = current_gui_workspace_schema_version;
    workspace.left_sidebar_ratio = normalize_splitter_ratio(workspace.left_sidebar_ratio, 0.52F);
    workspace.right_sidebar_ratio = normalize_splitter_ratio(workspace.right_sidebar_ratio, 0.48F);
    workspace.analysis_lower_ratio =
        normalize_splitter_ratio(workspace.analysis_lower_ratio, 0.56F);
    workspace.resources_events_ratio =
        normalize_splitter_ratio(workspace.resources_events_ratio, 0.42F);
}

GuiClearColor gui_theme_clear_color(GuiTheme theme) noexcept {
    return theme == GuiTheme::Light ? GuiClearColor{0.88F, 0.90F, 0.94F, 1.0F}
                                    : GuiClearColor{0.08F, 0.09F, 0.11F, 1.0F};
}

} // namespace cpssim
