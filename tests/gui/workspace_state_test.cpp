/*** Verify presentation-only workspace defaults and splitter normalization. ***/

#include "cpssim/gui/workspace_state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace cpssim;

TEST_CASE("workspace defaults expose every Goal 3 workbench panel", "[gui][workspace]") {
    const GuiWorkspaceState workspace;
    REQUIRE(workspace.theme == GuiTheme::Dark);
    REQUIRE(workspace.panels.explorer);
    REQUIRE(workspace.panels.system_builder);
    REQUIRE(workspace.panels.inspector);
    REQUIRE(workspace.panels.resources);
    REQUIRE(workspace.panels.events);
    REQUIRE(workspace.panels.results);
    REQUIRE(workspace.event_filters == GuiEventFilters{});
}

TEST_CASE("workspace splitter ratios clamp invalid and extreme values", "[gui][workspace]") {
    GuiWorkspaceState workspace;
    workspace.left_sidebar_ratio = -2.0F;
    workspace.right_sidebar_ratio = 4.0F;
    workspace.center_split_ratio = std::numeric_limits<float>::quiet_NaN();
    normalize_workspace_state(workspace);
    REQUIRE(workspace.left_sidebar_ratio == 0.05F);
    REQUIRE(workspace.right_sidebar_ratio == 0.95F);
    REQUIRE(workspace.center_split_ratio == 0.62F);
}

TEST_CASE("center tabs move deterministically and reset to the Goal 5 arrangement",
          "[gui][workspace][tabs]") {
    GuiWorkspaceState workspace;
    REQUIRE(center_tab_is_upper(workspace, GuiCenterTab::Timeline));
    REQUIRE(move_center_tab(workspace, GuiCenterTab::Timeline, false));
    REQUIRE_FALSE(center_tab_is_upper(workspace, GuiCenterTab::Timeline));
    REQUIRE(workspace.active_lower_tab == GuiCenterTab::Timeline);
    REQUIRE_FALSE(move_center_tab(workspace, GuiCenterTab::Timeline, false));
    reset_center_tab_arrangement(workspace);
    REQUIRE(workspace.upper_tabs ==
            std::vector<GuiCenterTab>{GuiCenterTab::Architecture, GuiCenterTab::Timeline,
                                      GuiCenterTab::Signals, GuiCenterTab::Results});
    REQUIRE(workspace.lower_tabs ==
            std::vector<GuiCenterTab>{GuiCenterTab::Resources, GuiCenterTab::Events});
}

TEST_CASE("workspace normalization repairs duplicate and missing center tabs",
          "[gui][workspace][tabs]") {
    GuiWorkspaceState workspace;
    workspace.upper_tabs = {GuiCenterTab::Architecture, GuiCenterTab::Architecture};
    workspace.lower_tabs.clear();
    normalize_workspace_state(workspace);
    REQUIRE(workspace.upper_tabs.front() == GuiCenterTab::Architecture);
    REQUIRE(workspace.upper_tabs.size() + workspace.lower_tabs.size() == 6);
}

TEST_CASE("vertical split preserves both panels in narrow layouts", "[gui][workspace][layout]") {
    const auto regular = calculate_vertical_split(600.0F, 6.0F, 0.7F, 120.0F, 150.0F);
    REQUIRE(regular.first_height >= 120.0F);
    REQUIRE(regular.second_height >= 150.0F);
    REQUIRE(regular.first_height + regular.second_height == 594.0F);

    const auto narrow = calculate_vertical_split(100.0F, 6.0F, 0.9F, 80.0F, 80.0F);
    REQUIRE(narrow.first_height > 0.0F);
    REQUIRE(narrow.second_height > 0.0F);
    REQUIRE(narrow.first_height + narrow.second_height == 94.0F);
}
