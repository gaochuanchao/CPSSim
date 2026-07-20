/***
 * File: src/cpssim/gui/display_scale.hpp
 * Purpose: Declare graphics-independent validation for GUI content scale,
 *          framebuffer density, and scaled font sizes.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 * Notes: Display scale is presentation state and never affects simulation
 *        timestamps, commands, or deterministic event ordering.
 ***/

#pragma once

#include "cpssim/gui/workspace_state.hpp"

namespace cpssim {

// Returns a finite positive scale, preserving the last valid value when a
// window system reports a transient invalid value during a monitor change.
float sanitize_gui_display_scale(float reported_scale, float fallback_scale = 1.0F);

// Returns a finite positive framebuffer density without changing a valid
// platform-provided value.
float sanitize_gui_framebuffer_scale(float reported_scale, float fallback_scale = 1.0F);

// Returns a finite font size of at least one pixel so Dear ImGui cannot
// quantize a zoom-scaled font bake request to zero.
float sanitize_gui_font_size(float requested_font_size);

// Reports a stable, user-visible content-scale change while ignoring tiny
// floating-point jitter and transient invalid reports.
bool gui_display_scale_changed(float current_scale, float reported_scale);

// A theme change always rebuilds the unscaled base style; a monitor change
// reapplies the current base exactly once.
bool gui_presentation_style_changed(GuiTheme applied_theme, GuiTheme requested_theme,
                                    float current_scale, float reported_scale);

} // namespace cpssim
