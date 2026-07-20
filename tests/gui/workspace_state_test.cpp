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
    REQUIRE(workspace.event_filters == GuiEventFilters{});
}

TEST_CASE("workspace splitter ratios clamp invalid and extreme values", "[gui][workspace]") {
    GuiWorkspaceState workspace;
    workspace.left_sidebar_ratio = -2.0F;
    workspace.right_sidebar_ratio = 4.0F;
    workspace.analysis_lower_ratio = std::numeric_limits<float>::quiet_NaN();
    normalize_workspace_state(workspace);
    REQUIRE(workspace.left_sidebar_ratio == 0.05F);
    REQUIRE(workspace.right_sidebar_ratio == 0.95F);
    REQUIRE(workspace.analysis_lower_ratio == 0.56F);
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
