/*** Qt adapters for run configuration and detached runtime inspection panels. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include <QAbstractTableModel>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableView;

namespace cpssim::qt {

class QtRunConfigurationWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtRunConfigurationWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void refresh();

  private:
    QtWorkbenchBridge& bridge_;
    QLabel* applied_{nullptr};
    QLineEdit* stop_tick_{nullptr};
    QPushButton* validate_{nullptr};
    QPushButton* apply_{nullptr};
    QLabel* diagnostics_{nullptr};
    bool refreshing_{false};
};

class QtRuntimeInspectorWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtRuntimeInspectorWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void refresh();

  private:
    QtWorkbenchBridge& bridge_;
    QWidget* properties_{nullptr};
};

class QtResourceTableModel final : public QAbstractTableModel {
    Q_OBJECT

  public:
    enum Column { Resource, Running, Ready, BusyTicks, IdleTicks, Utilization, ColumnCount };
    enum Role { ResourceIdRole = Qt::UserRole + 1, RunningJobRole };

    explicit QtResourceTableModel(QtWorkbenchBridge& bridge, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    void synchronize();
    const GuiResourceSnapshot* row_at(int row) const;

  private:
    QtWorkbenchBridge& bridge_;
    std::shared_ptr<const SimulationSnapshot> snapshot_;
    std::uint64_t generation_{};
};

class QtResourcesWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtResourcesWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    QtResourceTableModel& resource_model() noexcept { return *model_; }
    QTableView& table() noexcept { return *table_; }

  private:
    void select_runtime(const QModelIndex& index);
    void synchronize_selection();

    QtWorkbenchBridge& bridge_;
    QtResourceTableModel* model_{nullptr};
    QTableView* table_{nullptr};
};

class QtDiagnosticsWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtDiagnosticsWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void refresh();

  private:
    QtWorkbenchBridge& bridge_;
    QListWidget* list_{nullptr};
};

} // namespace cpssim::qt
