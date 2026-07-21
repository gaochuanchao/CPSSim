/*** Implement presentation-only workspace normalization and splitter geometry. ***/

#include "cpssim/gui/workspace_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

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
    workspace.center_split_ratio = normalize_splitter_ratio(workspace.center_split_ratio, 0.62F);
    workspace.results_summary_ratio =
        normalize_splitter_ratio(workspace.results_summary_ratio, 0.30F);
    workspace.results_timing_ratio =
        normalize_splitter_ratio(workspace.results_timing_ratio, 0.42F);
    normalize_architecture_workspace(workspace.architecture);
    if (workspace.fast_event_batch_size == 0) {
        workspace.fast_event_batch_size = 1000;
    }
    if (workspace.fast_tick_batch_size == 0) {
        workspace.fast_tick_batch_size = 1000;
    }
    workspace.plot_line_thickness = std::isfinite(workspace.plot_line_thickness)
                                        ? std::clamp(workspace.plot_line_thickness, 0.5F, 8.0F)
                                        : 1.5F;
    if (!std::isfinite(workspace.plot_y_min) || !std::isfinite(workspace.plot_y_max) ||
        workspace.plot_y_min >= workspace.plot_y_max) {
        workspace.plot_y_min = -1.0;
        workspace.plot_y_max = 1.0;
        workspace.plot_auto_y = true;
    }
    const std::array all_tabs{GuiCenterTab::Architecture, GuiCenterTab::Timeline,
                              GuiCenterTab::Signals,      GuiCenterTab::Results,
                              GuiCenterTab::Resources,    GuiCenterTab::Events};
    std::vector<GuiCenterTab> seen;
    auto normalize_group = [&seen](std::vector<GuiCenterTab>& group) {
        std::erase_if(group, [&seen](GuiCenterTab tab) {
            if (std::find(seen.begin(), seen.end(), tab) != seen.end()) {
                return true;
            }
            seen.push_back(tab);
            return false;
        });
    };
    normalize_group(workspace.upper_tabs);
    normalize_group(workspace.lower_tabs);
    for (const auto tab : all_tabs) {
        if (std::find(seen.begin(), seen.end(), tab) == seen.end()) {
            workspace.upper_tabs.push_back(tab);
        }
    }
    const auto repair_active = [](const std::vector<GuiCenterTab>& group, GuiCenterTab& active) {
        if (!group.empty() && std::find(group.begin(), group.end(), active) == group.end()) {
            active = group.front();
        }
    };
    repair_active(workspace.upper_tabs, workspace.active_upper_tab);
    repair_active(workspace.lower_tabs, workspace.active_lower_tab);
}

void reset_center_tab_arrangement(GuiWorkspaceState& workspace) {
    workspace.upper_tabs = {GuiCenterTab::Architecture, GuiCenterTab::Timeline,
                            GuiCenterTab::Signals, GuiCenterTab::Results};
    workspace.lower_tabs = {GuiCenterTab::Resources, GuiCenterTab::Events};
    workspace.active_upper_tab = GuiCenterTab::Architecture;
    workspace.active_lower_tab = GuiCenterTab::Resources;
    workspace.center_split_ratio = 0.62F;
}

bool move_center_tab(GuiWorkspaceState& workspace, GuiCenterTab tab, bool to_upper) {
    auto& source = to_upper ? workspace.lower_tabs : workspace.upper_tabs;
    auto& destination = to_upper ? workspace.upper_tabs : workspace.lower_tabs;
    const auto found = std::find(source.begin(), source.end(), tab);
    if (found == source.end()) {
        return false;
    }
    source.erase(found);
    destination.push_back(tab);
    auto& destination_active = to_upper ? workspace.active_upper_tab : workspace.active_lower_tab;
    auto& source_active = to_upper ? workspace.active_lower_tab : workspace.active_upper_tab;
    destination_active = tab;
    if (!source.empty() && source_active == tab) {
        source_active = source.front();
    }
    return true;
}

bool center_tab_is_upper(const GuiWorkspaceState& workspace, GuiCenterTab tab) noexcept {
    return std::find(workspace.upper_tabs.begin(), workspace.upper_tabs.end(), tab) !=
           workspace.upper_tabs.end();
}

GuiClearColor gui_theme_clear_color(GuiTheme theme) noexcept {
    return theme == GuiTheme::Light ? GuiClearColor{0.88F, 0.90F, 0.94F, 1.0F}
                                    : GuiClearColor{0.08F, 0.09F, 0.11F, 1.0F};
}

bool gui_property_layout_is_wide(float available_width, float font_size) noexcept {
    return std::isfinite(available_width) && std::isfinite(font_size) && font_size > 0.0F &&
           available_width >= 24.0F * font_size;
}

} // namespace cpssim
