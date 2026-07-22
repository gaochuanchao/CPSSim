/*** Draw assignment stripe, resource badge, and compact task timing. ***/
#include "apps/qt_gui/architecture_node_painter.hpp"

#include "apps/qt_gui/architecture_model.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/internal/AbstractNodeGeometry.hpp>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>

#include <algorithm>

namespace cpssim::qt {

void QtArchitectureNodePainter::paint(QPainter* painter, QtNodes::NodeGraphicsObject& node) const {
    QtNodes::DefaultNodePainter::paint(painter, node);
    const auto* model = dynamic_cast<const QtArchitectureGraphModel*>(&node.graphModel());
    const auto* presentation = model != nullptr ? model->task_presentation(node.nodeId()) : nullptr;
    if (presentation == nullptr) {
        return;
    }
    const auto size = node.nodeScene()->nodeGeometry().size(node.nodeId());
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (presentation->highlighted) {
        auto highlight = presentation->accent;
        highlight.setAlpha(42);
        painter->fillRect(QRectF{7.0, 3.0, static_cast<qreal>(size.width() - 10),
                                 static_cast<qreal>(size.height() - 6)},
                          highlight);
    }
    if (presentation->resource_id.has_value()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(presentation->accent);
        painter->drawRoundedRect(QRectF{2.0, 3.0, 6.0, static_cast<qreal>(size.height() - 6)}, 2.0,
                                 2.0);
    } else {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen{presentation->accent, 2.0, Qt::DashLine});
        painter->drawLine(QPointF{5.0, 5.0}, QPointF{5.0, static_cast<qreal>(size.height() - 5)});
    }

    const QFontMetrics metrics{painter->font()};
    const auto badge_width = std::clamp(metrics.horizontalAdvance(presentation->resource_name) + 14,
                                        58, size.width() - 36);
    const QRectF badge{static_cast<qreal>(size.width() - badge_width - 8), 8.0,
                       static_cast<qreal>(badge_width), 22.0};
    auto badge_color = presentation->accent;
    badge_color.setAlpha(210);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badge_color);
    painter->drawRoundedRect(badge, 5.0, 5.0);
    painter->setPen(badge_color.lightness() > 135 ? QColor{20, 24, 30} : QColor{245, 247, 250});
    painter->drawText(
        badge, Qt::AlignCenter,
        metrics.elidedText(presentation->resource_name, Qt::ElideRight, badge_width - 10));

    const auto execution = presentation->execution_time.has_value()
                               ? QString::number(*presentation->execution_time)
                               : QStringLiteral("—");
    const auto timing = QStringLiteral("T=%1  D=%2  C=%3 ticks")
                            .arg(presentation->period)
                            .arg(presentation->deadline)
                            .arg(execution);
    // Use the workbench text colour through the application palette for
    // theme-aware readability.
    const auto timing_color = presentation->assignment_valid
                                  ? QColor{}  // default (will use palette)
                                  : QColor{226, 110, 100};
    painter->setPen(timing_color.isValid()
                        ? timing_color
                        : QApplication::palette().color(QPalette::Text));
    painter->drawText(QRectF{14.0, static_cast<qreal>(size.height() - 28),
                             static_cast<qreal>(size.width() - 24), 20.0},
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(timing, Qt::ElideRight, size.width() - 28));
    painter->restore();
}

} // namespace cpssim::qt
