/*** Implement Qt analysis views over immutable presentation/completed data. ***/
#include "apps/qt_gui/analysis_widgets.hpp"

#include "cpssim/application/bosch_result_analysis.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace cpssim::qt {
namespace {

constexpr int plot_left_margin = 62;
constexpr int plot_right_margin = 16;

QString run_state_name(GuiRunState state) {
    switch (state) {
    case GuiRunState::NotConfigured:
        return "Not configured";
    case GuiRunState::Paused:
        return "Paused";
    case GuiRunState::Running:
        return "Running";
    case GuiRunState::Finished:
        return "Finished";
    }
    return "Unknown";
}

QString scalar_type_name(GuiSignalScalarType type) {
    switch (type) {
    case GuiSignalScalarType::Real:
        return "Real";
    case GuiSignalScalarType::Integer:
        return "Integer";
    case GuiSignalScalarType::Boolean:
        return "Boolean";
    }
    return "Unknown";
}

QString scalar_text(const GuiScalarValue& value) {
    return std::visit(
        [](const auto& scalar) {
            using Value = std::decay_t<decltype(scalar)>;
            if constexpr (std::is_same_v<Value, bool>) {
                return scalar ? QString{"true"} : QString{"false"};
            } else if constexpr (std::is_same_v<Value, std::int64_t>) {
                return QString::number(scalar);
            } else {
                return QString::number(scalar, 'g', 10);
            }
        },
        value);
}

void clear_layout(QLayout* layout) {
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        if (item->layout() != nullptr) {
            clear_layout(item->layout());
        }
        delete item;
    }
}

QLabel* value_label(const QString& value, QWidget* parent) {
    auto* label = new QLabel(value, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QColor series_color(std::size_t index) {
    static const std::array<QColor, 8> colors{
        QColor{51, 153, 255}, QColor{255, 128, 64}, QColor{80, 190, 110},  QColor{205, 90, 180},
        QColor{230, 190, 65}, QColor{80, 200, 200}, QColor{170, 130, 245}, QColor{220, 95, 95}};
    return colors[index % colors.size()];
}

} // namespace

QtTimelineCanvas::QtTimelineCanvas(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge} {
    setObjectName("view.timeline");
    setMinimumSize(240, 160);
    setMouseTracking(true);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { synchronize(); });
    connect(&bridge_, &QtWorkbenchBridge::runtimeSelectionChanged, this,
            qOverload<>(&QtTimelineCanvas::update));
    synchronize();
}

void QtTimelineCanvas::synchronize() {
    const auto snapshot = bridge_.application().presentation_snapshot();
    const auto generation = bridge_.application().presentation_generation();
    if (snapshot == nullptr) {
        cache_.clear();
        result_ = nullptr;
        ++build_count_;
    } else if (result_ == nullptr || generation_ != generation) {
        result_ = &cache_.update(snapshot->event_log, snapshot->experiment, snapshot->current_tick);
        generation_ = generation;
        ++build_count_;
    }
    update();
}

void QtTimelineCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    if (result_ == nullptr || !result_->valid()) {
        painter.setPen(palette().text().color());
        painter.drawText(rect(), Qt::AlignCenter,
                         result_ == nullptr
                             ? "No timeline data"
                             : QString::fromStdString(result_->diagnostics.front().message));
        return;
    }
    const auto& timeline = *result_->timeline;
    const auto plot_width = std::max(1, width() - plot_left_margin - plot_right_margin);
    const auto maximum_tick = std::max<Tick>(1, timeline.current_tick);
    const auto row_height =
        std::max(24, (height() - 24) / std::max(1, static_cast<int>(timeline.rows.size())));
    painter.setPen(palette().mid().color());
    for (std::size_t row = 0; row < timeline.rows.size(); ++row) {
        const auto y = 22 + static_cast<int>(row) * row_height;
        painter.drawText(QRect{4, y, plot_left_margin - 10, row_height}, Qt::AlignVCenter,
                         QString::fromStdString(timeline.rows[row].label));
        painter.drawLine(plot_left_margin, y + row_height - 1, width() - plot_right_margin,
                         y + row_height - 1);
        for (const auto& interval : timeline.rows[row].intervals) {
            const auto begin =
                plot_left_margin + static_cast<int>(static_cast<double>(interval.begin_tick) /
                                                    static_cast<double>(maximum_tick) * plot_width);
            const auto end_tick = interval.end_tick.value_or(timeline.current_tick);
            const auto end =
                plot_left_margin + static_cast<int>(static_cast<double>(end_tick) /
                                                    static_cast<double>(maximum_tick) * plot_width);
            const QRect bar{begin, y + 5, std::max(2, end - begin), row_height - 10};
            if (interval.kind == GuiTimelineIntervalKind::Running) {
                painter.setBrush(palette().highlight());
            } else {
                painter.setBrush(Qt::NoBrush);
            }
            painter.setPen(palette().highlight().color());
            painter.drawRect(bar);
        }
    }
    painter.setPen(palette().text().color());
    painter.drawText(QRect{plot_left_margin, 0, plot_width, 20}, Qt::AlignLeft,
                     QString{"0 → %1 ticks"}.arg(maximum_tick));
    for (const auto& marker : timeline.markers) {
        const auto x =
            plot_left_margin + static_cast<int>(static_cast<double>(marker.tick) /
                                                static_cast<double>(maximum_tick) * plot_width);
        const auto selected = bridge_.application().runtime_selection().event_sequence() ==
                              std::optional<EventSequence>{marker.sequence};
        painter.setPen(selected ? QPen{palette().highlight().color(), 3.0}
                                : QPen{palette().text().color(), 1.0});
        painter.drawLine(x, 18, x, height());
    }
}

void QtTimelineCanvas::mousePressEvent(QMouseEvent* event) {
    if (result_ == nullptr || !result_->valid() || result_->timeline->markers.empty()) {
        return;
    }
    const auto plot_width = std::max(1, width() - plot_left_margin - plot_right_margin);
    const auto maximum_tick = std::max<Tick>(1, result_->timeline->current_tick);
    const auto clicked_tick = static_cast<Tick>(std::llround(
        std::clamp(event->position().x() - plot_left_margin, 0.0, static_cast<double>(plot_width)) /
        static_cast<double>(plot_width) * static_cast<double>(maximum_tick)));
    const auto nearest = std::min_element(
        result_->timeline->markers.begin(), result_->timeline->markers.end(),
        [clicked_tick](const auto& left, const auto& right) {
            return std::abs(left.tick - clicked_tick) < std::abs(right.tick - clicked_tick);
        });
    if (nearest != result_->timeline->markers.end()) {
        bridge_.application().runtime_selection().select_event(nearest->sequence);
        bridge_.application().runtime_selection().select_tick(nearest->tick);
        bridge_.notify_runtime_selection_changed();
    }
}

QtSignalTableModel::QtSignalTableModel(QtWorkbenchBridge& bridge, QObject* parent)
    : QAbstractTableModel(parent), bridge_{bridge} {
    synchronize();
}

int QtSignalTableModel::rowCount(const QModelIndex& parent) const {
    const auto* model = signal_model();
    return parent.isValid() || model == nullptr ? 0 : static_cast<int>(model->series.size());
}

int QtSignalTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

const GuiSignalModel* QtSignalTableModel::signal_model() const {
    return result_ != nullptr && result_->valid() ? &*result_->model : nullptr;
}

const GuiSignalSeries* QtSignalTableModel::row_at(int row) const {
    const auto* model = signal_model();
    if (model == nullptr || row < 0 || static_cast<std::size_t>(row) >= model->series.size()) {
        return nullptr;
    }
    return &model->series[static_cast<std::size_t>(row)];
}

QVariant QtSignalTableModel::data(const QModelIndex& index, int role) const {
    const auto* series = row_at(index.row());
    if (series == nullptr || (role != Qt::DisplayRole && role != Qt::ToolTipRole)) {
        return {};
    }
    switch (index.column()) {
    case Signal:
        return QString::fromStdString(series->descriptor.path);
    case Type:
        return scalar_type_name(series->descriptor.id.scalar_type);
    case Unit:
        return QString::fromStdString(series->descriptor.unit);
    case Samples:
        return QVariant::fromValue(static_cast<qulonglong>(series->samples.size()));
    case Latest:
        return series->samples.empty() ? QVariant{} : scalar_text(series->samples.back().value);
    case ColumnCount:
        break;
    }
    return {};
}

QVariant QtSignalTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    static constexpr std::array<const char*, ColumnCount> labels{"Signal", "Type", "Unit",
                                                                 "Samples", "Latest"};
    return section >= 0 && section < ColumnCount ? labels[static_cast<std::size_t>(section)]
                                                 : QVariant{};
}

void QtSignalTableModel::synchronize() {
    const auto snapshot = bridge_.application().presentation_snapshot();
    const auto generation = bridge_.application().presentation_generation();
    if (result_ != nullptr && generation_ == generation) {
        return;
    }
    beginResetModel();
    if (snapshot == nullptr) {
        cache_.clear();
        result_ = nullptr;
    } else {
        result_ =
            &cache_.update(snapshot->functional_observations, snapshot->functional_signal_registry);
    }
    generation_ = generation;
    endResetModel();
}

class QtSignalPreviewCanvas final : public QWidget {
  public:
    QtSignalPreviewCanvas(QtWorkbenchBridge& bridge, QtSignalTableModel& model, QWidget* parent)
        : QWidget(parent), bridge_{bridge}, model_{model} {
        setMinimumHeight(160);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), palette().base());
        const auto* model = model_.signal_model();
        if (model == nullptr || bridge_.application().workspace().selected_signals.empty()) {
            painter.setPen(palette().text().color());
            painter.drawText(rect(), Qt::AlignCenter, "Select signals in the table to preview.");
            return;
        }
        const auto snapshot = bridge_.application().presentation_snapshot();
        const auto end_tick =
            snapshot == nullptr ? Tick{0} : std::max<Tick>(1, snapshot->current_tick);
        const QRectF plot{plot_left_margin, 12.0, static_cast<double>(std::max(1, width() - 78)),
                          static_cast<double>(std::max(1, height() - 34))};
        painter.setPen(palette().mid().color());
        painter.drawRect(plot);
        std::size_t color = 0;
        for (const auto& id : bridge_.application().workspace().selected_signals) {
            const auto* series = find_signal_series(*model, id);
            if (series == nullptr || series->samples.empty()) {
                continue;
            }
            const auto samples = downsample_signal(
                *series, {.begin_tick = 0,
                          .end_tick = end_tick,
                          .maximum_points = plot_point_budget(static_cast<float>(plot.width()))});
            auto minimum = std::numeric_limits<double>::infinity();
            auto maximum = -std::numeric_limits<double>::infinity();
            for (const auto& sample : samples) {
                minimum = std::min(minimum, gui_scalar_as_double(sample.value));
                maximum = std::max(maximum, gui_scalar_as_double(sample.value));
            }
            if (minimum == maximum) {
                minimum -= 0.5;
                maximum += 0.5;
            }
            QPainterPath path;
            for (std::size_t index = 0; index < samples.size(); ++index) {
                const auto x = plot.left() + static_cast<double>(samples[index].tick) /
                                                 static_cast<double>(end_tick) * plot.width();
                const auto value = gui_scalar_as_double(samples[index].value);
                const auto y =
                    plot.bottom() - (value - minimum) / (maximum - minimum) * plot.height();
                index == 0 ? path.moveTo(x, y) : path.lineTo(x, y);
            }
            painter.setPen(QPen{series_color(color++), 1.5});
            painter.drawPath(path);
        }
    }

  private:
    QtWorkbenchBridge& bridge_;
    QtSignalTableModel& model_;
};

QtSignalsWidget::QtSignalsWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, model_{new QtSignalTableModel(bridge, this)},
      table_{new QTableView(this)}, preview_{new QtSignalPreviewCanvas(bridge, *model_, this)} {
    setObjectName("view.signals");
    table_->setObjectName("signals.table");
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->horizontalHeader()->setStretchLastSection(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(table_, 2);
    layout->addWidget(preview_, 3);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { update_selected_signals(); });
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { synchronize(); });
    synchronize();
}

void QtSignalsWidget::synchronize() {
    model_->synchronize();
    const QSignalBlocker blocker{table_->selectionModel()};
    table_->clearSelection();
    const auto& selected = bridge_.application().workspace().selected_signals;
    for (int row = 0; row < model_->rowCount(); ++row) {
        const auto* series = model_->row_at(row);
        if (std::find(selected.begin(), selected.end(), series->descriptor.id) != selected.end()) {
            table_->selectionModel()->select(model_->index(row, 0), QItemSelectionModel::Select |
                                                                        QItemSelectionModel::Rows);
        }
    }
    preview_->update();
}

void QtSignalsWidget::update_selected_signals() {
    auto& selected = bridge_.application().workspace().selected_signals;
    selected.clear();
    const auto rows = table_->selectionModel()->selectedRows();
    for (const auto& row : rows) {
        if (const auto* series = model_->row_at(row.row()); series != nullptr) {
            selected.push_back(series->descriptor.id);
        }
    }
    preview_->update();
}

class QtIntegratedPlotCanvas final : public QWidget {
  public:
    explicit QtIntegratedPlotCanvas(QtWorkbenchBridge& bridge, QWidget* parent)
        : QWidget(parent), bridge_{bridge} {
        setMinimumHeight(220);
    }

    std::uint64_t build_count() const noexcept { return cache_.build_count(); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), palette().base());
        const auto* completed = bridge_.application().completed_result();
        if (completed == nullptr || !completed->result->signals.valid()) {
            painter.setPen(palette().text().color());
            painter.drawText(rect(), Qt::AlignCenter, "A completed run is required for plotting.");
            return;
        }
        auto& workspace = bridge_.application().workspace();
        const auto& result = *completed->result;
        if (workspace.selected_signals.empty() && completed->bosch_analysis != nullptr &&
            completed->bosch_analysis->lateral_error != nullptr) {
            workspace.selected_signals.push_back(
                completed->bosch_analysis->lateral_error->descriptor.id);
        }
        const auto range =
            resolve_plot_range(result, workspace.plot_range_mode,
                               bridge_.application().runtime_selection().tick_range(),
                               workspace.plot_custom_begin, workspace.plot_custom_end);
        static_cast<void>(cache_.update(completed->run_generation, *result.signals.model,
                                        workspace.selected_signals, workspace.plot_x_axis_unit,
                                        range, static_cast<float>(width())));
        const auto lanes = build_plot_lanes(*result.signals.model, workspace.selected_signals);
        if (lanes.empty()) {
            painter.setPen(palette().text().color());
            painter.drawText(rect(), Qt::AlignCenter, "Select one or more completed-run signals.");
            return;
        }
        const auto lane_height = std::max(80, height() / static_cast<int>(lanes.size()));
        for (std::size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
            const QRectF plot{
                plot_left_margin,
                static_cast<double>(lane_index * static_cast<std::size_t>(lane_height) + 18),
                static_cast<double>(std::max(1, width() - plot_left_margin - plot_right_margin)),
                static_cast<double>(std::max(1, lane_height - 34))};
            painter.setPen(palette().mid().color());
            if (workspace.plot_grid) {
                for (int grid = 0; grid <= 4; ++grid) {
                    const auto y = plot.top() + static_cast<double>(grid) / 4.0 * plot.height();
                    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
                }
            }
            painter.drawRect(plot);
            painter.setPen(palette().text().color());
            painter.drawText(QRectF{0, plot.top(), plot_left_margin - 5.0, 20.0}, Qt::AlignRight,
                             QString::fromStdString(lanes[lane_index].unit));
            const auto tick_x = [&](Tick tick) {
                const auto span = std::max<Tick>(1, range.end - range.begin);
                return plot.left() + static_cast<double>(tick - range.begin) /
                                         static_cast<double>(span) * plot.width();
            };
            if (completed->bosch_analysis != nullptr && workspace.plot_bosch_critical_sections) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor{230, 180, 60, 35});
                for (const auto& interval : visible_bosch_critical_intervals(
                         *completed->bosch_analysis, range.begin, range.end)) {
                    painter.drawRect(QRectF{
                        tick_x(interval.begin_tick), plot.top(),
                        std::max(1.0, tick_x(interval.end_tick) - tick_x(interval.begin_tick)),
                        plot.height()});
                }
                painter.setBrush(Qt::NoBrush);
            }
            auto minimum = std::numeric_limits<double>::infinity();
            auto maximum = -std::numeric_limits<double>::infinity();
            for (const auto* series : lanes[lane_index].series) {
                if (const auto* projection = cache_.find(series->descriptor.id);
                    projection != nullptr) {
                    for (const auto& sample : projection->samples) {
                        minimum = std::min(minimum, gui_scalar_as_double(sample.value));
                        maximum = std::max(maximum, gui_scalar_as_double(sample.value));
                    }
                }
            }
            if (!std::isfinite(minimum) || minimum == maximum) {
                minimum = std::isfinite(minimum) ? minimum - 0.5 : 0.0;
                maximum = std::isfinite(maximum) ? maximum + 0.5 : 1.0;
            }
            for (std::size_t series_index = 0; series_index < lanes[lane_index].series.size();
                 ++series_index) {
                const auto* series = lanes[lane_index].series[series_index];
                const auto* projection = cache_.find(series->descriptor.id);
                if (projection == nullptr || projection->samples.empty()) {
                    continue;
                }
                QPainterPath path;
                const auto span = std::max<Tick>(1, range.end - range.begin);
                for (std::size_t sample_index = 0; sample_index < projection->samples.size();
                     ++sample_index) {
                    const auto& sample = projection->samples[sample_index];
                    const auto x = plot.left() + static_cast<double>(sample.tick - range.begin) /
                                                     static_cast<double>(span) * plot.width();
                    const auto y = plot.bottom() - (gui_scalar_as_double(sample.value) - minimum) /
                                                       (maximum - minimum) * plot.height();
                    sample_index == 0 ? path.moveTo(x, y) : path.lineTo(x, y);
                }
                painter.setPen(QPen{series_color(series_index), workspace.plot_line_thickness});
                painter.drawPath(path);
                painter.drawText(
                    QPointF{plot.left() + 6.0,
                            plot.top() + 14.0 + static_cast<double>(series_index) * 14.0},
                    QString::fromStdString(series->descriptor.display_name));
            }
            if (completed->bosch_analysis != nullptr && workspace.plot_bosch_thresholds &&
                lanes[lane_index].unit == "m" && minimum < 0.2 && maximum > -0.2) {
                painter.setPen(QPen{QColor{220, 80, 80}, 1.0, Qt::DashLine});
                for (const auto threshold : {-0.2, 0.2}) {
                    const auto y =
                        plot.bottom() - (threshold - minimum) / (maximum - minimum) * plot.height();
                    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
                }
            }
            if (completed->bosch_analysis != nullptr && workspace.plot_bosch_deadline_misses) {
                painter.setPen(QPen{QColor{220, 70, 70}, 1.0, Qt::DotLine});
                for (const auto tick : completed->bosch_analysis->deadline_miss_ticks) {
                    if (tick >= range.begin && tick <= range.end) {
                        const auto x = tick_x(tick);
                        painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
                    }
                }
            }
            if (workspace.plot_selected_tick) {
                if (const auto selected = bridge_.application().runtime_selection().tick_range();
                    selected.has_value() && selected->begin_tick == selected->end_tick &&
                    selected->begin_tick >= range.begin && selected->begin_tick <= range.end) {
                    const auto x = tick_x(selected->begin_tick);
                    painter.setPen(QPen{palette().highlight().color(), 2.0});
                    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        const auto* completed = bridge_.application().completed_result();
        if (completed == nullptr) {
            return;
        }
        const auto range = resolve_plot_range(
            *completed->result, bridge_.application().workspace().plot_range_mode,
            bridge_.application().runtime_selection().tick_range(),
            bridge_.application().workspace().plot_custom_begin,
            bridge_.application().workspace().plot_custom_end);
        const auto width_value = std::max(1, width() - plot_left_margin - plot_right_margin);
        const auto fraction = std::clamp(event->position().x() - plot_left_margin, 0.0,
                                         static_cast<double>(width_value)) /
                              static_cast<double>(width_value);
        const auto tick =
            range.begin + static_cast<Tick>(std::llround(
                              fraction * static_cast<double>(range.end - range.begin)));
        bridge_.application().runtime_selection().select_tick(tick);
        bridge_.notify_runtime_selection_changed();
        update();
    }

  private:
    QtWorkbenchBridge& bridge_;
    GuiPlotDataCache cache_;
};

QtIntegratedPlotWidget::QtIntegratedPlotWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, status_{new QLabel(this)}, axis_{new QComboBox(this)},
      range_{new QComboBox(this)}, custom_begin_{new QLineEdit(this)},
      custom_end_{new QLineEdit(this)}, grid_{new QCheckBox("Grid", this)},
      canvas_{new QtIntegratedPlotCanvas(bridge, this)} {
    setObjectName("view.integratedPlot");
    axis_->addItems({"Ticks", "Seconds"});
    range_->addItems({"Full run", "Selected range", "Custom range"});
    custom_begin_->setPlaceholderText("Begin tick");
    custom_end_->setPlaceholderText("End tick");
    canvas_->setObjectName("plot.canvas");
    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel("X axis", this));
    controls->addWidget(axis_);
    controls->addWidget(new QLabel("Range", this));
    controls->addWidget(range_);
    controls->addWidget(custom_begin_);
    controls->addWidget(custom_end_);
    controls->addWidget(grid_);
    controls->addStretch();
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(status_);
    layout->addWidget(canvas_, 1);
    connect(axis_, &QComboBox::currentIndexChanged, this, [this](int index) {
        bridge_.application().workspace().plot_x_axis_unit =
            index == 0 ? GuiPlotXAxisUnit::Ticks : GuiPlotXAxisUnit::Seconds;
        canvas_->update();
    });
    connect(range_, &QComboBox::currentIndexChanged, this, [this](int index) {
        bridge_.application().workspace().plot_range_mode = static_cast<GuiPlotRangeMode>(index);
        const auto custom = index == static_cast<int>(GuiPlotRangeMode::Custom);
        custom_begin_->setEnabled(custom);
        custom_end_->setEnabled(custom);
        canvas_->update();
    });
    const auto custom_changed = [this] {
        bool begin_okay = false;
        bool end_okay = false;
        const auto begin = custom_begin_->text().toLongLong(&begin_okay);
        const auto end = custom_end_->text().toLongLong(&end_okay);
        if (begin_okay && end_okay && begin >= 0 && end >= begin) {
            bridge_.application().workspace().plot_custom_begin = begin;
            bridge_.application().workspace().plot_custom_end = end;
            canvas_->update();
        }
    };
    connect(custom_begin_, &QLineEdit::editingFinished, this, custom_changed);
    connect(custom_end_, &QLineEdit::editingFinished, this, custom_changed);
    connect(grid_, &QCheckBox::toggled, this, [this](bool enabled) {
        bridge_.application().workspace().plot_grid = enabled;
        canvas_->update();
    });
    connect(&bridge_, &QtWorkbenchBridge::completedResultChanged, this,
            &QtIntegratedPlotWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::runtimeSelectionChanged, canvas_,
            qOverload<>(&QtIntegratedPlotCanvas::update));
    refresh();
}

void QtIntegratedPlotWidget::refresh() {
    const auto ready = bridge_.application().completed_result() != nullptr;
    status_->setText(ready ? "Immutable completed-run data"
                           : "Integrated Plot is available after completion.");
    axis_->setEnabled(ready);
    range_->setEnabled(ready);
    custom_begin_->setEnabled(ready && bridge_.application().workspace().plot_range_mode ==
                                           GuiPlotRangeMode::Custom);
    custom_end_->setEnabled(custom_begin_->isEnabled());
    axis_->setCurrentIndex(
        bridge_.application().workspace().plot_x_axis_unit == GuiPlotXAxisUnit::Ticks ? 0 : 1);
    range_->setCurrentIndex(static_cast<int>(bridge_.application().workspace().plot_range_mode));
    custom_begin_->setText(QString::number(bridge_.application().workspace().plot_custom_begin));
    custom_end_->setText(QString::number(bridge_.application().workspace().plot_custom_end));
    grid_->setChecked(bridge_.application().workspace().plot_grid);
    canvas_->update();
}

std::uint64_t QtIntegratedPlotWidget::plot_build_count() const noexcept {
    return canvas_->build_count();
}

QtResultsWidget::QtResultsWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, plot_{new QPushButton("Open Integrated Plot", this)},
      export_{new QPushButton("Export Completed Results...", this)}, state_{new QLabel(this)},
      content_{new QWidget(this)} {
    setObjectName("view.results");
    plot_->setObjectName("results.openPlot");
    export_->setObjectName("results.export");
    state_->setObjectName("results.state");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(plot_);
    buttons->addWidget(export_);
    buttons->addStretch();
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(buttons);
    layout->addWidget(state_);
    layout->addWidget(content_, 1);
    connect(plot_, &QPushButton::clicked, this, &QtResultsWidget::openIntegratedPlotRequested);
    connect(export_, &QPushButton::clicked, this, &QtResultsWidget::export_completed);
    connect(&bridge_, &QtWorkbenchBridge::completedResultChanged, this, &QtResultsWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::progressChanged, this, &QtResultsWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this, &QtResultsWidget::refresh);
    refresh();
}

void QtResultsWidget::refresh() {
    const auto* completed = bridge_.application().completed_result();
    plot_->setEnabled(completed != nullptr);
    export_->setEnabled(completed != nullptr && bridge_.application().has_active_project());
    auto* layout = qobject_cast<QVBoxLayout*>(content_->layout());
    if (layout == nullptr) {
        layout = new QVBoxLayout(content_);
    } else {
        clear_layout(layout);
    }
    if (completed == nullptr) {
        const auto finalization = bridge_.application().finalization_state();
        state_->setText(finalization == CompletedResultFinalizationState::Finalizing
                            ? "Finalizing completed run..."
                            : QString{"Results are generated after completion. Tick %1 / %2 · %3"}
                                  .arg(bridge_.progress().current_tick)
                                  .arg(bridge_.progress().stop_tick)
                                  .arg(run_state_name(bridge_.run_state())));
        return;
    }
    state_->setText("Completed run analysis");
    const auto& metrics = completed->result->metrics;
    auto* summary = new QFormLayout;
    summary->setObjectName("results.summary");
    summary->addRow("Canonical events",
                    value_label(QString::number(metrics.event_count), content_));
    summary->addRow("Completed jobs",
                    value_label(QString::number(metrics.completed_jobs), content_));
    summary->addRow("Deadline misses",
                    value_label(QString::number(metrics.deadline_misses), content_));
    summary->addRow("Preemptions", value_label(QString::number(metrics.preemptions), content_));
    summary->addRow("Horizon",
                    value_label(QString{"%1 ticks"}.arg(metrics.horizon_tick), content_));
    layout->addLayout(summary);

    const auto paired = metrics.messages.delivery_delay.has_value()
                            ? metrics.messages.delivery_delay->count
                            : std::uint64_t{0};
    const auto undelivered = metrics.messages.sent - std::min(metrics.messages.sent, paired);
    const auto delay = metrics.messages.delivery_delay.has_value()
                           ? QString{"%1 / %2 / %3 ticks"}
                                 .arg(metrics.messages.delivery_delay->minimum)
                                 .arg(metrics.messages.delivery_delay->mean(), 0, 'f', 3)
                                 .arg(metrics.messages.delivery_delay->maximum)
                           : QString{"Unavailable"};
    const auto mode = completed->performance.mode == GuiRunMode::Fast ? "Fast" : "Live";
    const auto batch =
        completed->performance.batch_unit == GuiFastBatchUnit::Events ? "Events" : "Ticks";
    auto* timing = value_label(
        QString{"Messages: %1 sent · %2 delivered · %3 paired · %4 undelivered\n"
                "Delivery delay min/mean/max: %5\n"
                "Wall time: %6 s · %7 events/s · %8 ticks/s\nMode: %9 · %10/%11"}
            .arg(metrics.messages.sent)
            .arg(metrics.messages.delivered)
            .arg(paired)
            .arg(undelivered)
            .arg(delay)
            .arg(std::chrono::duration<double>(completed->performance.wall_clock_duration).count(),
                 0, 'f', 3)
            .arg(completed->performance.events_per_second, 0, 'f', 1)
            .arg(completed->performance.ticks_per_second, 0, 'f', 1)
            .arg(mode)
            .arg(batch)
            .arg(completed->performance.batch_size),
        content_);
    layout->addWidget(timing);
    if (completed->bosch_analysis != nullptr) {
        auto maximum_error = 0.0;
        if (completed->bosch_analysis->lateral_error != nullptr) {
            for (const auto& sample : completed->bosch_analysis->lateral_error->samples) {
                maximum_error =
                    std::max(maximum_error, std::abs(gui_scalar_as_double(sample.value)));
            }
        }
        layout->addWidget(value_label(
            QString{"Bosch Summary: %1 threshold crossings · %2 critical sections · maximum "
                    "absolute lateral error %3 m"}
                .arg(static_cast<qulonglong>(completed->bosch_analysis->threshold_crossings.size()))
                .arg(static_cast<qulonglong>(completed->bosch_analysis->critical_intervals.size()))
                .arg(maximum_error, 0, 'g', 8),
            content_));
    }
    auto* tasks = new QTableView(content_);
    tasks->setObjectName("results.tasks");
    auto* task_model = new QStandardItemModel(tasks);
    task_model->setHorizontalHeaderLabels(
        {"Task", "Completed jobs", "Minimum", "Mean", "Maximum", "Deadline", "Misses"});
    for (const auto& task : metrics.task_responses) {
        QList<QStandardItem*> row;
        row.push_back(new QStandardItem(QString{"%1 (T%2)"}
                                            .arg(QString::fromStdString(task.task_name))
                                            .arg(task.task_id.value())));
        row.push_back(new QStandardItem(QString::number(task.completed_jobs)));
        row.push_back(new QStandardItem(task.response_time.has_value()
                                            ? QString::number(task.response_time->minimum)
                                            : "Unavailable"));
        row.push_back(new QStandardItem(task.response_time.has_value()
                                            ? QString::number(task.response_time->mean(), 'f', 3)
                                            : "Unavailable"));
        row.push_back(new QStandardItem(task.response_time.has_value()
                                            ? QString::number(task.response_time->maximum)
                                            : "Unavailable"));
        row.push_back(new QStandardItem(QString::number(task.deadline)));
        row.push_back(new QStandardItem(QString::number(task.deadline_misses)));
        task_model->appendRow(row);
    }
    tasks->setModel(task_model);
    tasks->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tasks->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(tasks, 1);
}

void QtResultsWidget::export_completed() {
    if (!bridge_.application().has_active_project() ||
        bridge_.application().completed_result() == nullptr) {
        return;
    }
    const auto default_directory = bridge_.application().active_project().root() / "results";
    const auto destination = QFileDialog::getExistingDirectory(
        this, "Export Completed Results", QString::fromStdString(default_directory.string()));
    if (destination.isEmpty()) {
        return;
    }
    try {
        const auto* completed = bridge_.application().completed_result();
        RunScenarioMetadata scenario;
        std::vector<WorkbookControlMetric> control;
        if (completed->result->scenario_kind == "bosch") {
            scenario.bosch_trajectory = "trajectory";
            scenario.fmu_identity = "LateralMotionControl";
            control = bosch_workbook_control_metrics(*completed->result);
        }
        const auto run_id = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        const auto artifacts = bridge_.application().export_completed_result(
            {.destination_directory = destination.toStdString(),
             .run_id = run_id.toStdString(),
             .scope = RunExportScope::Complete,
             .selected_range = std::nullopt,
             .include_excel = true,
             .scenario = std::move(scenario),
             .control_metrics = std::move(control),
             .created_at_utc = {}});
        bridge_.application().set_status("Run results exported to " +
                                         artifacts.run_directory.string());
    } catch (const std::exception& error) {
        bridge_.application().set_status(error.what(), true);
    }
    Q_EMIT bridge_.statusChanged();
}

} // namespace cpssim::qt
