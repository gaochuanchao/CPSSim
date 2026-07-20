/***
 * File: src/cpssim/gui/display_scale.cpp
 * Purpose: Validate GUI scale values before they reach Dear ImGui's dynamic
 *          font rasterizer.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#include "cpssim/gui/display_scale.hpp"

#include <algorithm>
#include <cmath>

namespace cpssim {
namespace {

constexpr float default_display_scale = 1.0F;
constexpr float minimum_display_scale = 1.0F;
constexpr float maximum_display_scale = 4.0F;
constexpr float minimum_font_size = 1.0F;
constexpr float display_scale_change_epsilon = 0.01F;

bool valid_scale(float scale) { return std::isfinite(scale) && scale > 0.0F; }

} // namespace

float sanitize_gui_display_scale(float reported_scale, float fallback_scale) {
    if (!valid_scale(fallback_scale)) {
        fallback_scale = default_display_scale;
    }
    const auto selected_scale = valid_scale(reported_scale) ? reported_scale : fallback_scale;
    return std::clamp(selected_scale, minimum_display_scale, maximum_display_scale);
}

float sanitize_gui_framebuffer_scale(float reported_scale, float fallback_scale) {
    if (valid_scale(reported_scale)) {
        return reported_scale;
    }
    return valid_scale(fallback_scale) ? fallback_scale : default_display_scale;
}

float sanitize_gui_font_size(float requested_font_size) {
    return valid_scale(requested_font_size) ? std::max(requested_font_size, minimum_font_size)
                                            : minimum_font_size;
}

bool gui_display_scale_changed(float current_scale, float reported_scale) {
    const auto safe_current_scale = sanitize_gui_display_scale(current_scale);
    const auto safe_reported_scale = sanitize_gui_display_scale(reported_scale, safe_current_scale);
    return std::abs(safe_reported_scale - safe_current_scale) > display_scale_change_epsilon;
}

bool gui_presentation_style_changed(GuiTheme applied_theme, GuiTheme requested_theme,
                                    float current_scale, float reported_scale) {
    return applied_theme != requested_theme ||
           gui_display_scale_changed(current_scale, reported_scale);
}

} // namespace cpssim
