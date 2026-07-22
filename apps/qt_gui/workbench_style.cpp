/*** Derive stable, theme-adjusted resource colors from strong identities. ***/
#include "apps/qt_gui/workbench_style.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cpssim::qt {
namespace {

constexpr std::array<int, 12> resource_hues{205, 28, 145, 275, 52, 180, 330, 95, 235, 12, 165, 300};

std::size_t palette_index(ResourceId resource_id) {
    auto value = resource_id.value();
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    return static_cast<std::size_t>(value % resource_hues.size());
}

} // namespace

QColor resource_accent_color(ResourceId resource_id, GuiTheme theme) {
    const auto hue = resource_hues[palette_index(resource_id)];
    const auto saturation = theme == GuiTheme::Dark ? 185 : 170;
    const auto lightness = theme == GuiTheme::Dark ? 150 : 105;
    return QColor::fromHsl(hue, saturation, lightness);
}

QColor unassigned_accent_color(GuiTheme theme) {
    return theme == GuiTheme::Dark ? QColor{145, 150, 158} : QColor{105, 110, 118};
}

} // namespace cpssim::qt
