/*** CPSSim task-node decoration layered over QtNodes' standard painter. ***/
#pragma once

#include <QtNodes/DefaultNodePainter>

namespace cpssim::qt {

class QtArchitectureNodePainter final : public QtNodes::DefaultNodePainter {
  public:
    void paint(QPainter* painter, QtNodes::NodeGraphicsObject& node) const override;
};

} // namespace cpssim::qt
