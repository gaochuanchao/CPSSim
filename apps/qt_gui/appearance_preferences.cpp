/*** Persist application appearance outside project workspace state. ***/
#include "apps/qt_gui/appearance_preferences.hpp"

#include <QSettings>

namespace cpssim::qt {

GuiTheme QtAppearancePreferences::theme() const {
    QSettings settings{"CPSSim", "CPSSim Qt GUI"};
    return settings.value(qt_theme_preference_key, "dark").toString() == "light" ? GuiTheme::Light
                                                                                 : GuiTheme::Dark;
}

void QtAppearancePreferences::set_theme(GuiTheme theme) const {
    QSettings settings{"CPSSim", "CPSSim Qt GUI"};
    settings.setValue(qt_theme_preference_key, theme == GuiTheme::Light ? "light" : "dark");
}

} // namespace cpssim::qt
