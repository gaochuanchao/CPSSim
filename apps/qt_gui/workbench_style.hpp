/*** Centralized native-workbench visual identities. ***/
#pragma once

#include "cpssim/gui/workspace_state.hpp"

#include <QColor>
#include <QPalette>

namespace cpssim::qt {

QColor resource_accent_color(ResourceId resource_id, GuiTheme theme);
QColor unassigned_accent_color(GuiTheme theme);
QPalette workbench_palette(GuiTheme theme);
GuiTheme current_workbench_theme();
void apply_workbench_theme(GuiTheme theme);

} // namespace cpssim::qt
