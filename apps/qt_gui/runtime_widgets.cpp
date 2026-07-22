/*** Implement Qt forms/tables over detached workbench state. ***/
#include "apps/qt_gui/runtime_widgets.hpp"

#include "cpssim/gui/event_table_model.hpp"
#include "cpssim/gui/resource_presentation.hpp"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QStyleOptionProgressBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <exception>
#include <optional>

namespace cpssim::qt {
namespace {

QString job_text(JobIdentity job) {
    return QString{"T%1:J%2"}.arg(job.task_id().value()).arg(job.job_id().value());
}

template <typename Identifier> QString optional_id(const std::optional<Identifier>& identifier) {
    return identifier.has_value() ? QString::number(identifier->value()) : QString{"Unavailable"};
}

void clear_layout(QLayout* layout) {
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void add_property(QFormLayout* form, const QString& label, const QString& value) {
    auto* content = new QLabel(value);
    content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content->setWordWrap(true);
    form->addRow(label, content);
}

const Event* find_event(const SimulationSnapshot& snapshot, EventSequence sequence) {
    const auto found =
        std::find_if(snapshot.event_log.begin(), snapshot.event_log.end(),
                     [sequence](const auto& event) { return event.sequence() == sequence; });
    return found == snapshot.event_log.end() ? nullptr : &*found;
}

bool event_refers_to_job(const Event& event, JobIdentity job) {
    return event.entities().task_id == job.task_id() && event.entities().job_id == job.job_id();
}

class UtilizationDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (index.column() != QtResourceTableModel::Utilization ||
            !index.data(Qt::UserRole).isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionProgressBar progress;
        progress.rect = option.rect.adjusted(3, 3, -3, -3);
        progress.minimum = 0;
        progress.maximum = 100;
        progress.progress = index.data(Qt::UserRole).toInt();
        progress.text = QString{"%1%"}.arg(progress.progress);
        progress.textVisible = true;
        if (option.widget != nullptr) {
            option.widget->style()->drawControl(QStyle::CE_ProgressBar, &progress, painter,
                                                option.widget);
        }
    }
};

} // namespace

QtRunConfigurationWidget::QtRunConfigurationWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge} {
    setObjectName("view.runConfiguration");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    applied_ = new QLabel(this);
    stop_tick_ = new QLineEdit(this);
    stop_tick_->setObjectName("runConfiguration.stopTick");
    stop_tick_->setValidator(
        new QRegularExpressionValidator(QRegularExpression{"[0-9]+"}, stop_tick_));
    form->addRow("Currently applied", applied_);
    form->addRow("Policy", new QLabel("Fixed priority", this));
    form->addRow("Stop tick", stop_tick_);
    layout->addLayout(form);
    validate_ = new QPushButton("Validate changes", this);
    validate_->setObjectName("runConfiguration.validate");
    apply_ = new QPushButton("Apply and restart", this);
    apply_->setObjectName("runConfiguration.apply");
    layout->addWidget(validate_);
    layout->addWidget(apply_);
    auto* file_actions = new QHBoxLayout;
    load_ = new QPushButton("Load run plan...", this);
    load_->setObjectName("runConfiguration.load");
    save_ = new QPushButton("Save run plan...", this);
    save_->setObjectName("runConfiguration.save");
    file_actions->addWidget(load_);
    file_actions->addWidget(save_);
    layout->addLayout(file_actions);
    diagnostics_ = new QLabel(this);
    diagnostics_->setObjectName("runConfiguration.diagnostics");
    diagnostics_->setWordWrap(true);
    layout->addWidget(diagnostics_);
    layout->addStretch();

    connect(stop_tick_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_) {
            return;
        }
        bool okay = false;
        const auto value = stop_tick_->text().toLongLong(&okay);
        if (okay) {
            static_cast<void>(bridge_.set_stop_tick(value));
        }
        refresh();
    });
    connect(validate_, &QPushButton::clicked, &bridge_, &QtWorkbenchBridge::validate_changes);
    connect(apply_, &QPushButton::clicked, &bridge_, &QtWorkbenchBridge::apply_and_restart);
    connect(load_, &QPushButton::clicked, this, [this] {
        const auto selected = QFileDialog::getOpenFileName(
            this, "Load Run Plan", {}, "CPSSim run plans (*.json);;JSON files (*.json)");
        if (selected.isEmpty()) {
            return;
        }
        try {
            static_cast<void>(bridge_.load_run_plan(selected.toStdString()));
        } catch (const std::exception& error) {
            bridge_.application().set_status(error.what(), true);
            Q_EMIT bridge_.statusChanged();
        }
    });
    connect(save_, &QPushButton::clicked, this, [this] {
        const auto selected = QFileDialog::getSaveFileName(this, "Save Run Plan", "default.json",
                                                           "CPSSim run plans (*.json)");
        if (selected.isEmpty()) {
            return;
        }
        try {
            static_cast<void>(bridge_.save_run_plan(selected.toStdString()));
        } catch (const std::exception& error) {
            bridge_.application().set_status(error.what(), true);
            Q_EMIT bridge_.statusChanged();
        }
    });
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtRunConfigurationWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::runConfigurationChanged, this,
            &QtRunConfigurationWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtRunConfigurationWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::statusChanged, this, &QtRunConfigurationWidget::refresh);
    refresh();
}

void QtRunConfigurationWidget::refresh() {
    refreshing_ = true;
    const auto& application = bridge_.application();
    setEnabled(application.has_active_session());
    if (application.has_active_session()) {
        const auto& session = application.active_session();
        applied_->setText(session.active_plan() == nullptr
                              ? "Not configured"
                              : QString{"Stop tick %1"}.arg(session.active_plan()->stop_tick()));
        stop_tick_->setText(QString::number(session.draft().stop_tick()));
        const auto editable = session.draft_editable();
        stop_tick_->setEnabled(editable);
        validate_->setEnabled(editable);
        apply_->setEnabled(editable);
        load_->setEnabled(editable);
        save_->setEnabled(editable);
        diagnostics_->setText(application.system_changes_dirty()
                                  ? "Run Configuration — Unapplied changes"
                                  : QString::fromStdString(application.status()));
    }
    refreshing_ = false;
}

QtRuntimeInspectorWidget::QtRuntimeInspectorWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, properties_{new QWidget(this)} {
    setObjectName("view.runtimeInspector");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(properties_);
    connect(&bridge_, &QtWorkbenchBridge::runtimeSelectionChanged, this,
            &QtRuntimeInspectorWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this,
            [this](quint64) { refresh(); });
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtRuntimeInspectorWidget::refresh);
    refresh();
}

void QtRuntimeInspectorWidget::refresh() {
    auto* existing = properties_->layout();
    if (existing == nullptr) {
        existing = new QFormLayout(properties_);
    } else {
        clear_layout(existing);
    }
    auto* form = qobject_cast<QFormLayout*>(existing);
    const auto snapshot = bridge_.application().presentation_snapshot();
    if (snapshot == nullptr) {
        add_property(form, "Runtime", "No active presentation snapshot.");
        return;
    }
    const auto& selection = bridge_.application().runtime_selection();
    if (selection.tick_range().has_value()) {
        add_property(form, "Selected ticks",
                     QString{"%1 to %2"}
                         .arg(selection.tick_range()->begin_tick)
                         .arg(selection.tick_range()->end_tick));
    }
    if (selection.kind() == GuiSelectionKind::Resource && selection.resource_id().has_value()) {
        const auto found = std::find_if(
            snapshot->resources.begin(), snapshot->resources.end(),
            [&](const auto& resource) { return resource.id == *selection.resource_id(); });
        if (found != snapshot->resources.end()) {
            add_property(form, "Resource", QString::fromStdString(found->name));
            add_property(form, "Running",
                         found->running_job.has_value() ? job_text(*found->running_job) : "Idle");
            add_property(form, "Ready", QString::number(found->ready_jobs.size()));
            add_property(form, "Busy ticks", QString::number(found->busy_ticks));
            add_property(form, "Idle ticks", QString::number(found->idle_ticks));
        }
    } else if (selection.kind() == GuiSelectionKind::Event &&
               selection.event_sequence().has_value()) {
        const auto* event = find_event(*snapshot, *selection.event_sequence());
        if (event != nullptr) {
            add_property(form, "Sequence", QString::number(event->sequence().value()));
            add_property(form, "Tick", QString::number(event->tick()));
            add_property(form, "Type", gui_event_type_name(event->type()));
            add_property(form, "Phase", gui_event_phase_name(event->phase()));
            add_property(form, "Task", optional_id(event->entities().task_id));
            add_property(form, "Job", optional_id(event->entities().job_id));
            add_property(form, "Resource", optional_id(event->entities().resource_id));
            add_property(form, "Message", optional_id(event->entities().message_id));
            add_property(form, "Vehicle", optional_id(event->entities().vehicle_id));
            add_property(form, "Cause", optional_id(event->cause_sequence()));
            auto* raw = new QPlainTextEdit(
                QString::fromStdString(event_raw_json(*snapshot, event->sequence())), properties_);
            raw->setReadOnly(true);
            raw->setMaximumHeight(120);
            form->addRow("Raw JSON", raw);
        }
    } else if (selection.kind() == GuiSelectionKind::Job && selection.job().has_value()) {
        const auto job = *selection.job();
        std::optional<Tick> release;
        std::optional<Tick> start;
        std::optional<Tick> finish;
        std::optional<ResourceId> resource;
        auto deadline_missed = false;
        for (const auto& event : snapshot->event_log) {
            if (!event_refers_to_job(event, job)) {
                continue;
            }
            if (event.entities().resource_id.has_value()) {
                resource = event.entities().resource_id;
            }
            if (event.type() == EventType::JobRelease) {
                release = event.tick();
            } else if (event.type() == EventType::JobStart && !start.has_value()) {
                start = event.tick();
            } else if (event.type() == EventType::JobFinish) {
                finish = event.tick();
            } else if (event.type() == EventType::DeadlineMiss) {
                deadline_missed = true;
            }
        }
        auto lifecycle = QString{"Trace reference only"};
        if (finish.has_value()) {
            lifecycle = "Finished";
        } else if (deadline_missed) {
            lifecycle = "Deadline missed";
        } else {
            for (const auto& runtime_resource : snapshot->resources) {
                if (runtime_resource.running_job == job) {
                    lifecycle = "Running";
                } else if (std::find(runtime_resource.ready_jobs.begin(),
                                     runtime_resource.ready_jobs.end(),
                                     job) != runtime_resource.ready_jobs.end()) {
                    lifecycle = "Ready";
                }
            }
        }
        const auto tick_text = [](std::optional<Tick> value) {
            return value.has_value() ? QString::number(*value) : QString{"Unavailable"};
        };
        const auto* task = find_task(snapshot->experiment, job.task_id());
        const auto deadline = release.has_value() && task != nullptr
                                  ? std::optional<Tick>{*release + task->deadline}
                                  : std::nullopt;
        add_property(form, "Task", QString::number(job.task_id().value()));
        add_property(form, "Job", QString::number(job.job_id().value()));
        add_property(form, "Lifecycle", lifecycle);
        add_property(form, "Release tick", tick_text(release));
        add_property(form, "Start tick", tick_text(start));
        add_property(form, "Finish tick", tick_text(finish));
        add_property(form, "Deadline", tick_text(deadline));
        add_property(form, "Response time",
                     release.has_value() && finish.has_value()
                         ? QString{"%1 ticks"}.arg(*finish - *release)
                         : QString{"Unavailable"});
        add_property(form, "Resource", optional_id(resource));
    } else {
        add_property(form, "Selection", "Select an event, job, or runtime resource.");
    }
}

QtResourceTableModel::QtResourceTableModel(QtWorkbenchBridge& bridge, QObject* parent)
    : QAbstractTableModel(parent), bridge_{bridge} {
    synchronize();
}

int QtResourceTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || snapshot_ == nullptr ? 0
                                                    : static_cast<int>(snapshot_->resources.size());
}

int QtResourceTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

const GuiResourceSnapshot* QtResourceTableModel::row_at(int row) const {
    if (snapshot_ == nullptr || row < 0 ||
        static_cast<std::size_t>(row) >= snapshot_->resources.size()) {
        return nullptr;
    }
    return &snapshot_->resources[static_cast<std::size_t>(row)];
}

QVariant QtResourceTableModel::data(const QModelIndex& index, int role) const {
    const auto* resource = row_at(index.row());
    if (resource == nullptr) {
        return {};
    }
    if (role == ResourceIdRole) {
        return QVariant::fromValue(static_cast<qulonglong>(resource->id.value()));
    }
    if (role == RunningJobRole && resource->running_job.has_value()) {
        return job_text(*resource->running_job);
    }
    if (role == Qt::UserRole && index.column() == Utilization) {
        return static_cast<int>(
            calculate_resource_utilization(resource->busy_ticks, resource->idle_ticks) * 100.0 +
            0.5);
    }
    if (role == Qt::ToolTipRole && index.column() == Utilization) {
        const auto observed = resource->busy_ticks + resource->idle_ticks;
        return QString{"Busy %1 · Idle %2 · Observed %3 ticks"}
            .arg(resource->busy_ticks)
            .arg(resource->idle_ticks)
            .arg(observed);
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
    case Resource:
        return QString{"%1 (R%2)"}
            .arg(QString::fromStdString(resource->name))
            .arg(resource->id.value());
    case Running:
        return resource->running_job.has_value() ? job_text(*resource->running_job) : "Idle";
    case Ready:
        return QVariant::fromValue(static_cast<qulonglong>(resource->ready_jobs.size()));
    case BusyTicks:
        return QVariant::fromValue(static_cast<qlonglong>(resource->busy_ticks));
    case IdleTicks:
        return QVariant::fromValue(static_cast<qlonglong>(resource->idle_ticks));
    case Utilization:
        return resource->busy_ticks + resource->idle_ticks > 0 ? QVariant{0}
                                                               : QVariant{"No observations"};
    case ColumnCount:
        break;
    }
    return {};
}

QVariant QtResourceTableModel::headerData(int section, Qt::Orientation orientation,
                                          int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    static constexpr std::array<const char*, ColumnCount> labels{
        "Resource", "Running", "Ready", "Busy ticks", "Idle ticks", "Utilization"};
    return section >= 0 && section < ColumnCount ? labels[static_cast<std::size_t>(section)]
                                                 : QVariant{};
}

void QtResourceTableModel::synchronize() {
    const auto generation = bridge_.application().presentation_generation();
    if (snapshot_ != nullptr && generation_ == generation) {
        return;
    }
    beginResetModel();
    snapshot_ = bridge_.application().presentation_snapshot();
    generation_ = generation;
    endResetModel();
}

QtResourcesWidget::QtResourcesWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, model_{new QtResourceTableModel(bridge, this)},
      table_{new QTableView(this)} {
    setObjectName("view.resources");
    table_->setObjectName("resources.table");
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setItemDelegateForColumn(QtResourceTableModel::Utilization,
                                     new UtilizationDelegate(table_));
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(table_);
    connect(table_, &QTableView::clicked, this, &QtResourcesWidget::select_runtime);
    connect(&bridge_, &QtWorkbenchBridge::presentationChanged, this, [this](quint64) {
        model_->synchronize();
        synchronize_selection();
    });
    connect(&bridge_, &QtWorkbenchBridge::runtimeSelectionChanged, this,
            &QtResourcesWidget::synchronize_selection);
}

void QtResourcesWidget::select_runtime(const QModelIndex& index) {
    const auto* row = model_->row_at(index.row());
    if (row == nullptr) {
        return;
    }
    if (index.column() == QtResourceTableModel::Running && row->running_job.has_value()) {
        bridge_.application().runtime_selection().select_job(*row->running_job);
    } else {
        bridge_.application().runtime_selection().select_resource(row->id);
    }
    bridge_.notify_runtime_selection_changed();
}

void QtResourcesWidget::synchronize_selection() {
    const auto resource = bridge_.application().runtime_selection().resource_id();
    if (!resource.has_value()) {
        return;
    }
    for (int row = 0; row < model_->rowCount(); ++row) {
        if (model_->row_at(row)->id == *resource) {
            const QSignalBlocker blocker{table_->selectionModel()};
            table_->selectRow(row);
            return;
        }
    }
}

QtDiagnosticsWidget::QtDiagnosticsWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, list_{new QListWidget(this)} {
    setObjectName("view.diagnostics");
    list_->setObjectName("diagnostics.list");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(list_);
    connect(&bridge_, &QtWorkbenchBridge::statusChanged, this, &QtDiagnosticsWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtDiagnosticsWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::runConfigurationChanged, this,
            &QtDiagnosticsWidget::refresh);
    refresh();
}

void QtDiagnosticsWidget::refresh() {
    list_->clear();
    const auto& application = bridge_.application();
    if (!application.status().empty()) {
        list_->addItem(QString::fromStdString(application.status()));
    }
    for (const auto& diagnostic : application.system_validation().diagnostics) {
        list_->addItem(QString::fromStdString(diagnostic.message));
    }
    if (application.has_active_session() &&
        application.active_session().last_validation().has_value()) {
        for (const auto& diagnostic : application.active_session().last_validation()->diagnostics) {
            list_->addItem(QString::fromStdString(diagnostic.message));
        }
    }
    if (list_->count() == 0) {
        list_->addItem("No diagnostics.");
    }
}

} // namespace cpssim::qt
