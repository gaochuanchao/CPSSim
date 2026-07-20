/***
 * File: tests/gui/display_scale_test.cpp
 * Purpose: Verify safe display-scale fallback, bounds, and monitor-change
 *          detection without opening a graphics window.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#include "cpssim/gui/display_scale.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace {

using namespace cpssim;

TEST_CASE("GUI display scale preserves valid monitor reports", "[gui][display-scale]") {
    REQUIRE(sanitize_gui_display_scale(1.25F) == 1.25F);
    REQUIRE(sanitize_gui_display_scale(2.0F, 1.25F) == 2.0F);
}

TEST_CASE("GUI display scale falls back from transient invalid reports", "[gui][display-scale]") {
    REQUIRE(sanitize_gui_display_scale(0.0F, 1.5F) == 1.5F);
    REQUIRE(sanitize_gui_display_scale(-1.0F, 1.5F) == 1.5F);
    REQUIRE(sanitize_gui_display_scale(std::numeric_limits<float>::infinity(), 1.5F) == 1.5F);
    REQUIRE(sanitize_gui_display_scale(std::numeric_limits<float>::quiet_NaN(), 1.5F) == 1.5F);
    REQUIRE(sanitize_gui_display_scale(0.0F, 0.0F) == 1.0F);
}

TEST_CASE("GUI display scale stays inside supported presentation bounds", "[gui][display-scale]") {
    REQUIRE(sanitize_gui_display_scale(0.25F) == 1.0F);
    REQUIRE(sanitize_gui_display_scale(5.0F) == 4.0F);
}

TEST_CASE("GUI framebuffer scale preserves density and rejects zero", "[gui][display-scale]") {
    REQUIRE(sanitize_gui_framebuffer_scale(0.25F, 2.0F) == 0.25F);
    REQUIRE(sanitize_gui_framebuffer_scale(5.0F, 2.0F) == 5.0F);
    REQUIRE(sanitize_gui_framebuffer_scale(0.0F, 2.0F) == 2.0F);
    REQUIRE(sanitize_gui_framebuffer_scale(std::numeric_limits<float>::quiet_NaN(), 0.0F) == 1.0F);
}

TEST_CASE("GUI font size cannot quantize to a zero-pixel bake", "[gui][display-scale]") {
    REQUIRE(sanitize_gui_font_size(16.0F) == 16.0F);
    REQUIRE(sanitize_gui_font_size(0.4F) == 1.0F);
    REQUIRE(sanitize_gui_font_size(0.0F) == 1.0F);
    REQUIRE(sanitize_gui_font_size(std::numeric_limits<float>::quiet_NaN()) == 1.0F);
}

TEST_CASE("GUI display scale detects stable monitor changes", "[gui][display-scale]") {
    REQUIRE(gui_display_scale_changed(1.0F, 1.25F));
    REQUIRE_FALSE(gui_display_scale_changed(1.25F, 1.255F));
    REQUIRE_FALSE(gui_display_scale_changed(1.25F, 0.0F));
}

} // namespace
