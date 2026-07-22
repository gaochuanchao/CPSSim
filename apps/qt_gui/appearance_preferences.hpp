/*** Qt-only global appearance preferences. ***/
#pragma once

#include "cpssim/gui/workspace_state.hpp"

namespace cpssim::qt {

class QtAppearancePreferences {
  public:
    GuiTheme theme() const;
    void set_theme(GuiTheme theme) const;
};

inline constexpr auto qt_theme_preference_key = "appearance/theme_v1";

} // namespace cpssim::qt
