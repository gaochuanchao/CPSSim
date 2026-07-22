/*** Qt presentation adapters for timeline, signals, results, and native plotting. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/gui/plot_visualizer_model.hpp"
#include "cpssim/gui/timeline_model.hpp"

#include <QAbstractTableModel>
#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace cpssim::qt {

class QtTimelineCanvas final : public QWidget {
    Q_OBJECT

  public:
    explicit QtTimelineCanvas(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void synchronize();
    std::uint64_t build_count() const noexcept { return build_count_; }

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

  private:
    QtWorkbenchBridge& bridge_;
    GuiTimelineCache cache_;
    const GuiTimelineBuildResult* result_{nullptr};
    std::uint64_t generation_{};
    std::uint64_t build_count_{};
};

class QtSignalTableModel final : public QAbstractTableModel {
    Q_OBJECT

  public:
    enum Column { Signal, Type, Unit, Samples, Latest, ColumnCount };
    explicit QtSignalTableModel(QtWorkbenchBridge& bridge, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    void synchronize();
    const GuiSignalSeries* row_at(int row) const;
    const GuiSignalModel* signal_model() const;

  private:
    QtWorkbenchBridge& bridge_;
    GuiSignalCache cache_;
    const GuiSignalBuildResult* result_{nullptr};
    std::uint64_t generation_{};
};

class QtSignalPreviewCanvas;

class QtSignalsWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtSignalsWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    QtSignalTableModel& signal_model() noexcept { return *model_; }

  private:
    void synchronize();
    void update_selected_signals();

    QtWorkbenchBridge& bridge_;
    QtSignalTableModel* model_{nullptr};
    QTableView* table_{nullptr};
    QtSignalPreviewCanvas* preview_{nullptr};
};

class QtIntegratedPlotCanvas;

class QtIntegratedPlotWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtIntegratedPlotWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void refresh();
    std::uint64_t plot_build_count() const noexcept;

  private:
    QtWorkbenchBridge& bridge_;
    QLabel* status_{nullptr};
    QComboBox* axis_{nullptr};
    QComboBox* range_{nullptr};
    QLineEdit* custom_begin_{nullptr};
    QLineEdit* custom_end_{nullptr};
    QCheckBox* grid_{nullptr};
    QtIntegratedPlotCanvas* canvas_{nullptr};
};

class QtResultsWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtResultsWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);
    void refresh();

  Q_SIGNALS:
    void openIntegratedPlotRequested();

  private:
    void export_completed();

    QtWorkbenchBridge& bridge_;
    QPushButton* plot_{nullptr};
    QPushButton* export_{nullptr};
    QLabel* state_{nullptr};
    QWidget* content_{nullptr};
};

} // namespace cpssim::qt
