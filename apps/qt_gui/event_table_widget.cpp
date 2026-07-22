/*** Implement virtualized Canonical Events presentation and navigation. ***/
#include "apps/qt_gui/event_table_widget.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <chrono>
#include <optional>

namespace cpssim::qt {
namespace {

template <typename Identifier>
QVariant optional_identifier(const std::optional<Identifier>& value) {
    return value.has_value() ? QVariant::fromValue(static_cast<qulonglong>(value->value()))
                             : QVariant{};
}

template <typename Identifier> std::optional<Identifier> filter_identifier(const QLineEdit* edit) {
    if (edit->text().trimmed().isEmpty()) {
        return std::nullopt;
    }
    bool okay = false;
    const auto value = edit->text().toULongLong(&okay);
    return okay ? std::optional<Identifier>{Identifier{value}} : std::nullopt;
}

} // namespace

QtCanonicalEventTableModel::QtCanonicalEventTableModel(QtWorkbenchBridge& bridge, QObject* parent)
    : QAbstractTableModel(parent), bridge_{bridge} {
    synchronize();
}

int QtCanonicalEventTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(cache_.filtered_indices().size());
}

int QtCanonicalEventTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

const GuiEventTableRow* QtCanonicalEventTableModel::row_at(int filtered_row) const {
    if (filtered_row < 0 ||
        static_cast<std::size_t>(filtered_row) >= cache_.filtered_indices().size()) {
        return nullptr;
    }
    return &cache_.rows()[cache_.filtered_indices()[static_cast<std::size_t>(filtered_row)]];
}

QVariant QtCanonicalEventTableModel::data(const QModelIndex& index, int role) const {
    const auto* row = row_at(index.row());
    if (row == nullptr || (role != Qt::DisplayRole && role != Qt::ToolTipRole)) {
        return {};
    }
    switch (index.column()) {
    case Sequence:
        return QVariant::fromValue(static_cast<qulonglong>(row->sequence.value()));
    case TickColumn:
        return QVariant::fromValue(static_cast<qlonglong>(row->tick));
    case Time:
        return QString::number(row->time_milliseconds, 'f', 4) + " ms";
    case Type:
        return QString::fromStdString(row->type_name);
    case Phase:
        return QString::fromStdString(row->phase_name);
    case Task:
        return optional_identifier(row->entities.task_id);
    case Job:
        return optional_identifier(row->entities.job_id);
    case Resource:
        return optional_identifier(row->entities.resource_id);
    case Message:
        return optional_identifier(row->entities.message_id);
    case Vehicle:
        return optional_identifier(row->entities.vehicle_id);
    case Cause:
        return optional_identifier(row->cause);
    case ColumnCount:
        break;
    }
    return {};
}

QVariant QtCanonicalEventTableModel::headerData(int section, Qt::Orientation orientation,
                                                int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    static constexpr std::array<const char*, ColumnCount> labels{
        "Sequence", "Tick",     "Time",    "Type",    "Phase", "Task",
        "Job",      "Resource", "Message", "Vehicle", "Cause"};
    return section >= 0 && section < ColumnCount ? labels[static_cast<std::size_t>(section)]
                                                 : QVariant{};
}

void QtCanonicalEventTableModel::synchronize() {
    const auto snapshot = bridge_.application().presentation_snapshot();
    if (snapshot == nullptr) {
        beginResetModel();
        cache_ = {};
        generation_.reset();
        endResetModel();
        return;
    }
    const auto generation = bridge_.application().presentation_generation();
    if (generation_ != generation) {
        beginResetModel();
        static_cast<void>(cache_.update_rows(generation, *snapshot));
        static_cast<void>(cache_.update_filter(filters_, std::chrono::steady_clock::now()));
        generation_ = generation;
        endResetModel();
    }
}

void QtCanonicalEventTableModel::set_filters(GuiEventFilters filters, bool debounce_text) {
    filters_ = std::move(filters);
    static_cast<void>(debounce_text);
    refresh_filter(std::chrono::steady_clock::now());
}

void QtCanonicalEventTableModel::refresh_filter(std::chrono::steady_clock::time_point now) {
    beginResetModel();
    static_cast<void>(cache_.update_filter(filters_, now));
    endResetModel();
}

const GuiEventTableRow* QtCanonicalEventTableModel::row_by_sequence(EventSequence sequence) const {
    const auto row = find_event_row_by_sequence(cache_.rows(), sequence);
    return row.has_value() ? &cache_.rows()[*row] : nullptr;
}

std::optional<int>
QtCanonicalEventTableModel::filtered_row_for_sequence(EventSequence sequence) const {
    for (std::size_t filtered = 0; filtered < cache_.filtered_indices().size(); ++filtered) {
        if (cache_.rows()[cache_.filtered_indices()[filtered]].sequence == sequence) {
            return static_cast<int>(filtered);
        }
    }
    return std::nullopt;
}

QtCanonicalEventsWidget::QtCanonicalEventsWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, model_{new QtCanonicalEventTableModel(bridge, this)},
      table_{new QTableView(this)}, search_{new QLineEdit(this)}, type_filter_{new QComboBox(this)},
      task_filter_{new QLineEdit(this)}, resource_filter_{new QLineEdit(this)},
      vehicle_filter_{new QLineEdit(this)}, debounce_timer_{new QTimer(this)} {
    setObjectName("view.canonicalEvents");
    search_->setObjectName("events.search");
    search_->setPlaceholderText("Search canonical events");
    type_filter_->setObjectName("events.typeFilter");
    type_filter_->addItem("All types", -1);
    for (const auto type : {EventType::JobRelease, EventType::JobStart, EventType::JobPreempt,
                            EventType::JobResume, EventType::JobFinish, EventType::DeadlineMiss,
                            EventType::MessageSend, EventType::MessageDelivery}) {
        type_filter_->addItem(gui_event_type_name(type), static_cast<int>(type));
    }
    task_filter_->setPlaceholderText("Task ID");
    resource_filter_->setPlaceholderText("Resource ID");
    vehicle_filter_->setPlaceholderText("Vehicle ID");
    table_->setObjectName("events.table");
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setSortingEnabled(false);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionsMovable(true);
    table_->horizontalHeader()->setStretchLastSection(false);

    auto* filters = new QHBoxLayout;
    filters->addWidget(search_, 2);
    filters->addWidget(type_filter_);
    filters->addWidget(task_filter_);
    filters->addWidget(resource_filter_);
    filters->addWidget(vehicle_filter_);
    auto* columns = new QToolButton(this);
    columns->setText("Columns");
    columns->setPopupMode(QToolButton::InstantPopup);
    auto* column_menu = new QMenu(columns);
    columns->setMenu(column_menu);
    auto& visibility = bridge_.application().workspace().event_columns;
    const std::array<std::pair<const char*, bool*>, QtCanonicalEventTableModel::ColumnCount>
        column_settings{{{"Sequence", &visibility.sequence},
                         {"Tick", &visibility.tick},
                         {"Time", &visibility.time},
                         {"Type", &visibility.type},
                         {"Phase", &visibility.phase},
                         {"Task", &visibility.task},
                         {"Job", &visibility.job},
                         {"Resource", &visibility.resource},
                         {"Message", &visibility.message},
                         {"Vehicle", &visibility.vehicle},
                         {"Cause", &visibility.cause}}};
    for (std::size_t column = 0; column < column_settings.size(); ++column) {
        auto* action = column_menu->addAction(column_settings[column].first);
        action->setCheckable(true);
        action->setChecked(*column_settings[column].second);
        table_->setColumnHidden(static_cast<int>(column), !*column_settings[column].second);
        connect(action, &QAction::toggled, this,
                [this, column, value = column_settings[column].second](bool enabled) {
                    *value = enabled;
                    table_->setColumnHidden(static_cast<int>(column), !enabled);
                });
    }
    filters->addWidget(columns);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(filters);
    layout->addWidget(table_);

    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(160);
    connect(search_, &QLineEdit::textChanged, this, [this] {
        update_filters(true);
        debounce_timer_->start();
    });
    connect(debounce_timer_, &QTimer::timeout, this, [this] { update_filters(false); });
    for (auto* edit : {task_filter_, resource_filter_, vehicle_filter_}) {
        connect(edit, &QLineEdit::editingFinished, this, [this] { update_filters(false); });
    }
    connect(type_filter_, &QComboBox::currentIndexChanged, this, [this] { update_filters(false); });
    connect(table_, &QTableView::clicked, this, &QtCanonicalEventsWidget::select_runtime_event);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this, [this](quint64) {
        model_->synchronize();
        synchronize_selection();
    });
    connect(&bridge_, &QtWorkbenchBridge::runtimeSelectionChanged, this,
            &QtCanonicalEventsWidget::synchronize_selection);
    const auto& persisted = bridge_.application().workspace().event_filters;
    search_->setText(QString::fromStdString(persisted.text));
    if (persisted.type.has_value()) {
        type_filter_->setCurrentIndex(type_filter_->findData(static_cast<int>(*persisted.type)));
    }
    if (persisted.task.has_value()) {
        task_filter_->setText(QString::number(persisted.task->value()));
    }
    if (persisted.resource.has_value()) {
        resource_filter_->setText(QString::number(persisted.resource->value()));
    }
    if (persisted.vehicle.has_value()) {
        vehicle_filter_->setText(QString::number(persisted.vehicle->value()));
    }
    update_filters(false);
}

void QtCanonicalEventsWidget::update_filters(bool debounce_text) {
    GuiEventFilters filters;
    filters.text = search_->text().toStdString();
    if (type_filter_->currentData().toInt() >= 0) {
        filters.type = static_cast<EventType>(type_filter_->currentData().toInt());
    }
    filters.task = filter_identifier<TaskId>(task_filter_);
    filters.resource = filter_identifier<ResourceId>(resource_filter_);
    filters.vehicle = filter_identifier<VehicleId>(vehicle_filter_);
    bridge_.application().workspace().event_filters = filters;
    model_->set_filters(std::move(filters), debounce_text);
    synchronize_selection();
}

void QtCanonicalEventsWidget::select_runtime_event(const QModelIndex& index) {
    const auto* row = model_->row_at(index.row());
    if (row == nullptr) {
        return;
    }
    auto selected = row->sequence;
    auto selected_tick = row->tick;
    if (index.column() == QtCanonicalEventTableModel::Cause && row->cause.has_value()) {
        selected = *row->cause;
        if (const auto* cause_row = model_->row_by_sequence(selected); cause_row != nullptr) {
            selected_tick = cause_row->tick;
        }
    }
    bridge_.application().runtime_selection().select_event(selected);
    bridge_.application().runtime_selection().select_tick(selected_tick);
    bridge_.notify_runtime_selection_changed();
}

void QtCanonicalEventsWidget::synchronize_selection() {
    const auto sequence = bridge_.application().runtime_selection().event_sequence();
    if (!sequence.has_value()) {
        return;
    }
    const auto row = model_->filtered_row_for_sequence(*sequence);
    if (row.has_value()) {
        const QSignalBlocker blocker{table_->selectionModel()};
        table_->selectRow(*row);
        table_->scrollTo(model_->index(*row, 0));
    }
}

} // namespace cpssim::qt
