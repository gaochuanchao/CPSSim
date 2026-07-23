/*** Qt model/view Experiment Explorer over structural draft identities. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include <QWidget>

class QStandardItemModel;
class QTreeView;

namespace cpssim::qt {

class QtSystemBuilderWidget;

class QtExperimentExplorerWidget final : public QWidget {
    Q_OBJECT

  public:
    QtExperimentExplorerWidget(QtWorkbenchBridge& bridge, QtSystemBuilderWidget& builder,
                               QWidget* parent = nullptr);

    QTreeView& tree() noexcept { return *tree_; }
    void refresh();

  private:
    void apply_current_selection();
    void show_context_menu(const QPoint& position);
    void schedule_selection_sync();
    void synchronize_selection();

    QtWorkbenchBridge& bridge_;
    QtSystemBuilderWidget& builder_;
    QTreeView* tree_{nullptr};
    QStandardItemModel* model_{nullptr};
    bool refreshing_{false};
    bool selection_sync_pending_{false};
};

} // namespace cpssim::qt
