/*** Derive stable, theme-adjusted resource colors from strong identities. ***/
#include "apps/qt_gui/workbench_style.hpp"

#include <QApplication>
#include <QString>

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

QPalette workbench_palette(GuiTheme theme) {
    QPalette palette;
    if (theme == GuiTheme::Light) {
        palette.setColor(QPalette::Window, QColor{245, 246, 248});
        palette.setColor(QPalette::WindowText, QColor{28, 31, 35});
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor{235, 238, 242});
        palette.setColor(QPalette::Text, QColor{28, 31, 35});
        palette.setColor(QPalette::Button, QColor{240, 242, 245});
        palette.setColor(QPalette::ButtonText, QColor{28, 31, 35});
        palette.setColor(QPalette::Highlight, QColor{31, 103, 194});
        palette.setColor(QPalette::HighlightedText, Qt::white);
        return palette;
    }
    const QColor window{38, 41, 46};
    const QColor base{29, 31, 35};
    const QColor alternate{47, 51, 57};
    const QColor text{232, 234, 237};
    const QColor disabled{130, 134, 140};
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, text);
    palette.setColor(QPalette::ToolTipText, base);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, window);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor{53, 132, 228});
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    return palette;
}

GuiTheme current_workbench_theme() {
    return qApp != nullptr && qApp->property("cpssimTheme").toString() == "light" ? GuiTheme::Light
                                                                                  : GuiTheme::Dark;
}

void apply_workbench_theme(GuiTheme theme) {
    if (qApp == nullptr) {
        return;
    }

    qApp->setProperty(
        "cpssimTheme",
        theme == GuiTheme::Light ? "light" : "dark");

    QApplication::setStyle("Fusion");
    QApplication::setPalette(workbench_palette(theme));
}

} // namespace cpssim::qt
