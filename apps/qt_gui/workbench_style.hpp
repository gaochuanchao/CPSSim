/*** Centralized native-workbench visual identities. ***/
#pragma once

#include "cpssim/gui/workspace_state.hpp"

#include <QColor>

namespace cpssim::qt {

QColor resource_accent_color(ResourceId resource_id, GuiTheme theme);
QColor unassigned_accent_color(GuiTheme theme);

} // namespace cpssim::qt
