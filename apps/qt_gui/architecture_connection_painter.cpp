/*** Implement custom connection painter for dashed/solid link rendering. ***/
#include "apps/qt_gui/architecture_connection_painter.hpp"

#include "apps/qt_gui/architecture_model.hpp"

#include <QtNodes/ConnectionStyle>
#include <QtNodes/StyleCollection>
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace cpssim::qt {

QtArchitectureConnectionPainter::QtArchitectureConnectionPainter(
    const QtArchitectureGraphModel& model)
    : model_{model} {}

void QtArchitectureConnectionPainter::paint(QPainter* painter,
                                            const QtNodes::ConnectionGraphicsObject& cgo) const {
    const auto& connStyle = QtNodes::StyleCollection::connectionStyle();
    const auto cid = cgo.connectionId();

    // Determine if this is a Logical connection (dashed) or Communication (solid).
    const auto gui_conn = model_.connection_for(cid);
    const bool logical = gui_conn.has_value() && gui_conn->kind == GuiConnectionKind::Logical;

    // Endpoints
    const QPointF& in = cgo.endPoint(QtNodes::PortType::In);
    const QPointF& out = cgo.endPoint(QtNodes::PortType::Out);
    const auto c1c2 = cgo.pointsC1C2();

    QPainterPath cubic(out);
    cubic.cubicTo(c1c2.first, c1c2.second, in);

    // Hovered / selection highlight (wide background)
    const bool hovered = cgo.connectionState().hovered();
    const bool selected = cgo.isSelected();
    if (hovered || selected) {
        const double lineWidth = connStyle.lineWidth();
        QPen haloPen;
        haloPen.setWidthF(2.0 * lineWidth);
        haloPen.setColor(selected ? connStyle.selectedHaloColor() : connStyle.hoveredColor());
        if (logical) {
            haloPen.setStyle(Qt::DashLine);
        }
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(cubic);
    }

    // Main line
    {
        const double lineWidth = connStyle.lineWidth();
        QPen pen;
        pen.setWidthF(lineWidth);
        pen.setColor(selected ? connStyle.selectedColor() : connStyle.normalColor());
        if (logical) {
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({4.0, 3.0});
        }
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(cubic);
    }
}

QPainterPath
QtArchitectureConnectionPainter::getPainterStroke(const QtNodes::ConnectionGraphicsObject& cgo) const {
    const QPointF& in = cgo.endPoint(QtNodes::PortType::In);
    const QPointF& out = cgo.endPoint(QtNodes::PortType::Out);
    const auto c1c2 = cgo.pointsC1C2();

    QPainterPath cubic(out);
    cubic.cubicTo(c1c2.first, c1c2.second, in);

    // Use a wide stroke so dashed links remain easy to click.
    const auto& connStyle = QtNodes::StyleCollection::connectionStyle();
    QPainterPathStroker stroker;
    stroker.setWidth(2.0 * connStyle.lineWidth());
    return stroker.createStroke(cubic);
}

} // namespace cpssim::qt
