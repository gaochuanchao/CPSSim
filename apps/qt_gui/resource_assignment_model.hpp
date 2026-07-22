/*** Model/view resource-assignment editor over the shared system draft. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include <QAbstractTableModel>
#include <QTableView>
#include <QWidget>

#include <optional>
#include <vector>

class QHBoxLayout;

namespace cpssim::qt {

struct QtResourceAssignmentRow {
    TaskId task_id;
    QString task_name;
    std::optional<ResourceId> resource_id;
    QString resource_name;
    std::optional<Tick> execution_time;
    Priority priority{};
    bool accessible{false};
    QString status;
};

class QtResourceAssignmentModel final : public QAbstractTableModel {
  public:
    enum Column { Swatch, Task, Resource, Profile, PriorityColumn, Accessibility, Status, Count };
    enum Role { TaskIdRole = Qt::UserRole + 1, ResourceIdRole };

    explicit QtResourceAssignmentModel(QtWorkbenchBridge& bridge, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void rebuild();
    const QtResourceAssignmentRow* row_at(int row) const;

  private:
    QtWorkbenchBridge& bridge_;
    std::vector<QtResourceAssignmentRow> rows_;
    int sort_column_{Task};
    Qt::SortOrder sort_order_{Qt::AscendingOrder};
};

class QtResourceAssignmentsWidget final : public QWidget {
  public:
    explicit QtResourceAssignmentsWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);

    QtResourceAssignmentModel& assignment_model() noexcept { return model_; }
    QTableView& table() noexcept { return *table_; }
    void rebuild();

  private:
    void rebuild_legend();
    void select_table_row();

    QtWorkbenchBridge& bridge_;
    QtResourceAssignmentModel model_;
    QHBoxLayout* legend_{nullptr};
    QTableView* table_{nullptr};
};

} // namespace cpssim::qt
