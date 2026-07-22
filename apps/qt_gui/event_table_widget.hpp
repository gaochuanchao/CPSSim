/*** Qt lazy table adapter for the graphics-independent canonical-event cache. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/gui/event_table_model.hpp"

#include <QAbstractTableModel>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableView;
class QTimer;

namespace cpssim::qt {

class QtCanonicalEventTableModel final : public QAbstractTableModel {
    Q_OBJECT

  public:
    enum Column {
        Sequence,
        TickColumn,
        Time,
        Type,
        Phase,
        Task,
        Job,
        Resource,
        Message,
        Vehicle,
        Cause,
        ColumnCount,
    };

    explicit QtCanonicalEventTableModel(QtWorkbenchBridge& bridge, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void synchronize();
    void set_filters(GuiEventFilters filters, bool debounce_text);
    const GuiEventTableRow* row_at(int filtered_row) const;
    const GuiEventTableRow* row_by_sequence(EventSequence sequence) const;
    std::optional<int> filtered_row_for_sequence(EventSequence sequence) const;
    std::uint64_t row_build_count() const noexcept { return cache_.row_build_count(); }
    std::uint64_t filter_build_count() const noexcept { return cache_.filter_build_count(); }

  private:
    void refresh_filter(std::chrono::steady_clock::time_point now);

    QtWorkbenchBridge& bridge_;
    GuiEventTableCache cache_;
    GuiEventFilters filters_;
    std::optional<std::uint64_t> generation_;
};

class QtCanonicalEventsWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtCanonicalEventsWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);

    QtCanonicalEventTableModel& event_model() noexcept { return *model_; }
    QTableView& table() noexcept { return *table_; }

  private:
    void update_filters(bool debounce_text);
    void select_runtime_event(const QModelIndex& index);
    void synchronize_selection();

    QtWorkbenchBridge& bridge_;
    QtCanonicalEventTableModel* model_{nullptr};
    QTableView* table_{nullptr};
    QLineEdit* search_{nullptr};
    QComboBox* type_filter_{nullptr};
    QLineEdit* task_filter_{nullptr};
    QLineEdit* resource_filter_{nullptr};
    QLineEdit* vehicle_filter_{nullptr};
    QTimer* debounce_timer_{nullptr};
};

} // namespace cpssim::qt
