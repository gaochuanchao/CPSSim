/*** Custom QtNodes AbstractConnectionPainter for CPSSim Architecture.
 *  Communication links are solid; Logical links are dashed.
 ***/
#pragma once

#include <QtNodes/internal/AbstractConnectionPainter.hpp>
#include <QtNodes/internal/Definitions.hpp>

#include <unordered_map>

namespace QtNodes {
class ConnectionGraphicsObject;
struct ConnectionId;
} // namespace QtNodes

namespace cpssim::qt {

class QtArchitectureGraphModel;

class QtArchitectureConnectionPainter final : public QtNodes::AbstractConnectionPainter {
  public:
    explicit QtArchitectureConnectionPainter(const QtArchitectureGraphModel& model);

    void paint(QPainter* painter, const QtNodes::ConnectionGraphicsObject& cgo) const override;
    QPainterPath getPainterStroke(const QtNodes::ConnectionGraphicsObject& cgo) const override;

  private:
    const QtArchitectureGraphModel& model_;
};

} // namespace cpssim::qt
