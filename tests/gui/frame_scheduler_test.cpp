/*** Verify pure activity, waiting, redraw, and pointer-region policy. ***/

#include "cpssim/gui/frame_scheduler.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cpssim;

TEST_CASE("GUI activity chooses polling timeout and indefinite waits", "[gui][frames]") {
    REQUIRE(classify_gui_frame_activity({.run_state = GuiRunState::Running}) ==
            GuiFrameActivity::Running);
    REQUIRE(gui_wait_strategy(GuiFrameActivity::Running) == GuiWaitStrategy::Poll);
    REQUIRE(gui_wait_strategy(GuiFrameActivity::Interactive) == GuiWaitStrategy::Poll);
    REQUIRE(gui_wait_strategy(GuiFrameActivity::BackgroundPending) ==
            GuiWaitStrategy::WaitWithTimeout);
    REQUIRE(gui_wait_strategy(GuiFrameActivity::FullyIdle) == GuiWaitStrategy::WaitIndefinitely);
}

TEST_CASE("boundary pointer regions redraw only on crossings", "[gui][frames][pointer]") {
    GuiPointerRegionMap regions;
    regions.begin_frame();
    regions.add({7, {10.0F, 10.0F, 20.0F, 20.0F}, GuiPointerRegionBehavior::BoundarySensitive});
    regions.publish();
    GuiPointerRedrawPolicy policy;
    REQUIRE_FALSE(policy.cursor_moved({0.0F, 0.0F}, regions));
    REQUIRE(policy.cursor_moved({12.0F, 12.0F}, regions));
    REQUIRE_FALSE(policy.cursor_moved({13.0F, 13.0F}, regions));
    REQUIRE(policy.cursor_moved({30.0F, 30.0F}, regions));
}

TEST_CASE("position regions and stale maps conservatively redraw", "[gui][frames][pointer]") {
    GuiPointerRegionMap regions;
    GuiPointerRedrawPolicy policy;
    REQUIRE(policy.cursor_moved({1.0F, 1.0F}, regions));
    regions.begin_frame();
    regions.add({9, {0.0F, 0.0F, 20.0F, 20.0F}, GuiPointerRegionBehavior::PositionSensitive});
    regions.publish();
    REQUIRE(policy.cursor_moved({2.0F, 2.0F}, regions));
    REQUIRE(policy.cursor_moved({3.0F, 3.0F}, regions));
}

TEST_CASE("passive pointer regions do not redraw on movement or exit", "[gui][frames][pointer]") {
    GuiPointerRegionMap regions;
    regions.begin_frame();
    regions.add({11, {0.0F, 0.0F, 20.0F, 20.0F}, GuiPointerRegionBehavior::Passive});
    regions.publish();
    GuiPointerRedrawPolicy policy;
    REQUIRE_FALSE(policy.cursor_moved({2.0F, 2.0F}, regions));
    REQUIRE_FALSE(policy.cursor_moved({3.0F, 3.0F}, regions));
    REQUIRE_FALSE(policy.cursor_moved({30.0F, 30.0F}, regions));
}

TEST_CASE("redraw tracker acknowledges generations", "[gui][frames]") {
    GuiRedrawTracker redraw;
    REQUIRE(redraw.pending());
    redraw.acknowledge();
    REQUIRE_FALSE(redraw.pending());
    redraw.request();
    REQUIRE(redraw.pending());
}
