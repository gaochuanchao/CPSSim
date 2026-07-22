/*** Implement domain-backed Qt System Builder editors and undo commands. ***/
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace cpssim::qt {
namespace {

std::optional<std::size_t> resource_index(const EditableSystemDraft& draft, ResourceId id) {
    const auto found = std::find_if(draft.resources().begin(), draft.resources().end(),
                                    [id](const auto& row) { return row.id == id; });
    return found == draft.resources().end() ? std::nullopt
                                            : std::optional<std::size_t>{static_cast<std::size_t>(
                                                  std::distance(draft.resources().begin(), found))};
}

std::optional<std::size_t> task_index(const EditableSystemDraft& draft, TaskId id) {
    const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                    [id](const auto& row) { return row.id == id; });
    return found == draft.tasks().end() ? std::nullopt
                                        : std::optional<std::size_t>{static_cast<std::size_t>(
                                              std::distance(draft.tasks().begin(), found))};
}

std::optional<std::size_t> route_index(const EditableSystemDraft& draft, DraftMessageRouteKey key) {
    const auto found =
        std::find_if(draft.routes().begin(), draft.routes().end(), [key](const auto& route) {
            return route.source_task_id == key.source_task_id &&
                   route.destination_task_id == key.destination_task_id;
        });
    return found == draft.routes().end() ? std::nullopt
                                         : std::optional<std::size_t>{static_cast<std::size_t>(
                                               std::distance(draft.routes().begin(), found))};
}

std::optional<DraftMessageRouteKey> selected_route(const StructuralSelection& selection) {
    if (selection.kind() == StructuralSelectionKind::MessageRoute) {
        return selection.message_route();
    }
    if (selection.kind() == StructuralSelectionKind::Connection) {
        const auto connection = selection.connection();
        if (connection.has_value() && connection->kind == GuiConnectionKind::Communication) {
            return DraftMessageRouteKey{connection->source_task_id,
                                        connection->destination_task_id};
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> unsigned_value(const QLineEdit* edit) {
    bool okay = false;
    const auto value = edit->text().toULongLong(&okay);
    return okay ? std::optional<std::uint64_t>{value} : std::nullopt;
}

std::optional<Tick> tick_value(const QLineEdit* edit) {
    bool okay = false;
    const auto value = edit->text().toLongLong(&okay);
    return okay ? std::optional<Tick>{value} : std::nullopt;
}

QWidget* form_page(QWidget* parent, QFormLayout*& form) {
    auto* page = new QWidget(parent);
    form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return page;
}

QLabel* diagnostic_label(QWidget* parent, const char* object_name) {
    auto* label = new QLabel(parent);
    label->setObjectName(object_name);
    label->setWordWrap(true);
    label->setStyleSheet("color: palette(highlight);");
    return label;
}

} // namespace

QtTaskExecutionProfileEditModel::QtTaskExecutionProfileEditModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int QtTaskExecutionProfileEditModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int QtTaskExecutionProfileEditModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : Count;
}

QVariant QtTaskExecutionProfileEditModel::data(const QModelIndex& index, int role) const {
    const auto* row = row_at(index.row());
    if (row == nullptr) {
        return {};
    }
    switch (index.column()) {
    case Resource:
        if (role == Qt::DisplayRole) {
            return row->resource_name;
        }
        return {};
    case Accessible:
        if (role == Qt::CheckStateRole) {
            return row->accessible ? Qt::Checked : Qt::Unchecked;
        }
        return {};
    case ExecutionTime:
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return row->execution_time.has_value()
                       ? QVariant{static_cast<qlonglong>(*row->execution_time)}
                       : QVariant{};
        }
        return {};
    case Status:
        if (role == Qt::DisplayRole) {
            if (!row->accessible) {
                return QStringLiteral("Inaccessible");
            }
            return row->execution_time.has_value() ? QStringLiteral("Complete")
                                                   : QStringLiteral("Missing WCET");
        }
        return {};
    default:
        return {};
    }
}

QVariant QtTaskExecutionProfileEditModel::headerData(int section, Qt::Orientation orientation,
                                                     int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case Resource:
        return QStringLiteral("Resource");
    case Accessible:
        return QStringLiteral("Accessible");
    case ExecutionTime:
        return QStringLiteral("Execution time / WCET");
    case Status:
        return QStringLiteral("Status");
    default:
        return {};
    }
}

Qt::ItemFlags QtTaskExecutionProfileEditModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (!index.isValid()) {
        return result;
    }
    if (index.column() == Accessible) {
        result |= Qt::ItemIsUserCheckable;
    }
    if (index.column() == ExecutionTime) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool QtTaskExecutionProfileEditModel::setData(const QModelIndex& index, const QVariant& value,
                                              int role) {
    auto* row = const_cast<QtTaskExecutionProfileRow*>(row_at(index.row()));
    if (row == nullptr) {
        return false;
    }
    if (index.column() == Accessible && role == Qt::CheckStateRole) {
        row->accessible = (value.toInt() == Qt::Checked);
        if (!row->accessible) {
            row->execution_time.reset();
        }
        Q_EMIT dataChanged(index, index, {Qt::CheckStateRole, Qt::DisplayRole});
        // Also update Status and ExecutionTime cells
        const auto topLeft = createIndex(index.row(), ExecutionTime);
        const auto bottomRight = createIndex(index.row(), Status);
        Q_EMIT dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    if (index.column() == ExecutionTime && role == Qt::EditRole) {
        const auto text = value.toString().trimmed();
        if (text.isEmpty()) {
            row->execution_time.reset();
        } else {
            bool okay = false;
            const auto parsed = text.toLongLong(&okay);
            if (!okay || parsed < 0) {
                return false;
            }
            row->execution_time = parsed;
            row->accessible = true;
        }
        Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        // Also update Status and Accessible cells
        const auto topLeft = createIndex(index.row(), Accessible);
        const auto bottomRight = createIndex(index.row(), Status);
        Q_EMIT dataChanged(topLeft, bottomRight, {Qt::CheckStateRole, Qt::DisplayRole});
        return true;
    }
    return false;
}

void QtTaskExecutionProfileEditModel::load(TaskId task_id, const EditableSystemDraft& draft) {
    beginResetModel();
    rows_.clear();
    for (const auto& resource : draft.resources()) {
        const auto profile = draft.execution_profile(task_id, resource.id);
        rows_.push_back({resource.id, QString::fromStdString(resource.name),
                         profile.has_value(), profile});
    }
    endResetModel();
}

bool QtTaskExecutionProfileEditModel::complete() const {
    return std::all_of(rows_.begin(), rows_.end(), [](const auto& row) {
        return !row.accessible || row.execution_time.has_value();
    });
}

const QtTaskExecutionProfileRow* QtTaskExecutionProfileEditModel::row_at(int row) const {
    return row >= 0 && row < static_cast<int>(rows_.size()) ? &rows_[static_cast<std::size_t>(row)]
                                                            : nullptr;
}

QtSystemBuilderWidget::QtSystemBuilderWidget(QtWorkbenchBridge& bridge,
                                         QtStructuralEditController& edits,
                                         QWidget* parent)
    : QWidget(parent), bridge_{bridge}, edits_{edits} {
    setObjectName("view.systemBuilder");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("systemBuilder.scrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* editor = new QWidget(scroll);
    auto* editor_layout = new QVBoxLayout(editor);
    pages_ = new QStackedWidget(editor);
    pages_->setObjectName("systemBuilder.pages");
    build_pages();
    editor_layout->addWidget(pages_);
    diagnostic_summary_ = new QLabel(editor);
    diagnostic_summary_->setObjectName("systemBuilder.diagnostics");
    diagnostic_summary_->setWordWrap(true);
    editor_layout->addWidget(diagnostic_summary_);
    editor_layout->addStretch();
    scroll->setWidget(editor);
    layout->addWidget(scroll);

    connect_editors();
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtSystemBuilderWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtSystemBuilderWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtSystemBuilderWidget::refresh);
    refresh();
}

void QtSystemBuilderWidget::build_pages() {
    QFormLayout* form = nullptr;
    empty_page_ = form_page(pages_, form);
    empty_message_ = new QLabel("Select a system entity in Explorer or Architecture.", empty_page_);
    empty_message_->setWordWrap(true);
    form->addRow(empty_message_);

    system_page_ = form_page(pages_, form);
    /*
    * Keep fields beside their labels when space permits.
    * Move fields below their labels when the dock becomes narrow.
    */
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    tick_period_ = new QLineEdit(system_page_);
    tick_period_->setObjectName("systemBuilder.tickPeriod");
    tick_period_->setFixedWidth(140);
    preemption_ = new QComboBox(system_page_);
    preemption_->setObjectName("systemBuilder.preemption");
    preemption_->setFixedWidth(140);
    preemption_->addItems({"Preemptive", "Non-preemptive"});
    form->addRow("Tick period (ns):", tick_period_);
    form->addRow("Preemption mode:", preemption_);
    system_diagnostic_ = diagnostic_label(system_page_, "systemBuilder.systemDiagnostic");
    form->addRow(system_diagnostic_);

    resource_page_ = form_page(pages_, form);
    resource_id_ = new QLineEdit(resource_page_);
    resource_name_ = new QLineEdit(resource_page_);
    resource_id_->setObjectName("systemBuilder.resourceId");
    resource_id_->setFixedWidth(50);
    resource_name_->setObjectName("systemBuilder.resourceName");
    resource_name_->setFixedWidth(100);
    form->addRow("Resource ID:", resource_id_);
    form->addRow("Name:", resource_name_);
    resource_diagnostic_ = diagnostic_label(resource_page_, "systemBuilder.resourceDiagnostic");
    form->addRow(resource_diagnostic_);

    task_page_ = form_page(pages_, form);
    task_id_ = new QLineEdit(task_page_);
    task_name_ = new QLineEdit(task_page_);
    task_period_ = new QLineEdit(task_page_);
    task_deadline_ = new QLineEdit(task_page_);
    task_offset_ = new QLineEdit(task_page_);
    task_priority_ = new QLineEdit(task_page_);
    task_assignment_ = new QComboBox(task_page_);
    task_assignment_->setObjectName("systemBuilder.taskAssignment");
    
    task_id_->setFixedWidth(100);
    task_name_->setFixedWidth(100);
    task_period_->setFixedWidth(100);
    task_deadline_->setFixedWidth(100);
    task_offset_->setFixedWidth(100);
    task_priority_->setFixedWidth(100);
    task_assignment_->setFixedWidth(100);

    assignment_status_ = new QLabel(task_page_);
    assignment_status_->setObjectName("systemBuilder.assignmentStatus");
    profile_button_ =
        new QPushButton(
            QStringLiteral("Edit Execution Profile"),
            task_page_);
    profile_button_->setObjectName(
        "systemBuilder.editExecutionProfile");
    profile_button_->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed);
    profile_button_->setMinimumHeight(32);
    profile_button_->setFlat(false);
    profile_button_->setAutoDefault(false);
    profile_button_->setDefault(false);

    protected_help_ = new QLabel(task_page_);
    protected_help_->setWordWrap(true);
    task_id_->setObjectName("systemBuilder.taskId");
    task_name_->setObjectName("systemBuilder.taskName");
    form->addRow("Task ID:", task_id_);
    form->addRow("Name:", task_name_);
    form->addRow("Period:", task_period_);
    form->addRow("Deadline:", task_deadline_);
    form->addRow("Offset:", task_offset_);
    form->addRow("Priority:", task_priority_);
    form->addRow("Resource:", task_assignment_);
    form->addRow("Status:", assignment_status_);
    form->addRow(profile_button_);

    form->setAlignment(profile_button_, Qt::AlignLeft);

    /* No stylesheet — using native Fusion button appearance. */

    form->addRow(protected_help_);
    task_diagnostic_ = diagnostic_label(task_page_, "systemBuilder.taskDiagnostic");
    form->addRow(task_diagnostic_);

    connection_page_ = form_page(pages_, form);
    connection_source_ = new QComboBox(connection_page_);
    connection_destination_ = new QComboBox(connection_page_);
    connection_kind_ = new QLabel(connection_page_);
    connection_latency_ = new QLabel(connection_page_);
    route_send_offset_ = new QLineEdit(connection_page_);
    route_delay_ = new QLineEdit(connection_page_);
    form->addRow("Source:", connection_source_);
    form->addRow("Destination:", connection_destination_);
    form->addRow("Kind:", connection_kind_);
    form->addRow("Displayed latency:", connection_latency_);
    form->addRow("Send offset:", route_send_offset_);
    form->addRow("Delay:", route_delay_);
    auto* help = new QLabel(
        "Logical dependencies create no network events. Bosch communication displays 80 ticks; "
        "the adapter-owned one-tick handoff is hidden.",
        connection_page_);
    help->setWordWrap(true);
    form->addRow(help);
    connection_diagnostic_ =
        diagnostic_label(connection_page_, "systemBuilder.connectionDiagnostic");
    form->addRow(connection_diagnostic_);

    pages_->addWidget(empty_page_);
    pages_->addWidget(system_page_);
    pages_->addWidget(resource_page_);
    pages_->addWidget(task_page_);
    pages_->addWidget(connection_page_);
}

void QtSystemBuilderWidget::connect_editors() {
    auto* unsigned_validator = new QRegularExpressionValidator(QRegularExpression{"[0-9]+"}, this);
    for (auto* edit : {tick_period_, resource_id_, task_id_, task_period_, task_deadline_,
                       task_offset_, task_priority_, route_send_offset_, route_delay_}) {
        edit->setValidator(unsigned_validator);
    }
    connect(tick_period_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_ || !editing_enabled())
            return;
        const auto value = tick_value(tick_period_);
        if (!value.has_value())
            return;
        edits_.apply("Change tick period",
                      [value](auto& draft, auto&, auto&) { draft.set_tick_period_ns(*value); });
    });
    connect(preemption_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_ || !editing_enabled())
            return;
        edits_.apply("Change preemption mode", [index](auto& draft, auto&, auto&) {
            draft.set_preemption_mode(index == 0 ? PreemptionMode::Preemptive
                                                 : PreemptionMode::NonPreemptive);
        });
    });
    connect(resource_name_, &QLineEdit::editingFinished, this, [this] {
        const auto selected = bridge_.application().structural_selection().resource_id();
        if (refreshing_ || !editing_enabled() || !selected.has_value())
            return;
        const auto name = resource_name_->text().toStdString();
        edits_.apply("Rename resource", [selected, name](auto& draft, auto&, auto&) {
            if (const auto index = resource_index(draft, *selected); index.has_value())
                draft.set_resource_name(*index, name);
        });
    });
    connect(resource_id_, &QLineEdit::editingFinished, this, [this] {
        const auto selected = bridge_.application().structural_selection().resource_id();
        const auto value = unsigned_value(resource_id_);
        if (refreshing_ || !editing_enabled() || !selected.has_value() || !value.has_value())
            return;
        const ResourceId replacement{*value};
        edits_.apply("Change resource ID", [selected, replacement](auto& draft, auto& assignments,
                                                                    auto& selection) {
            if (const auto index = resource_index(draft, *selected); index.has_value()) {
                draft.set_resource_id(*index, replacement);
                for (auto& assignment : assignments)
                    if (assignment.resource_id == selected)
                        assignment.resource_id = replacement;
                selection.select_resource(replacement);
            }
        });
    });

    const auto task_text_edit = [this](QLineEdit* source, const QString& text) {
        connect(source, &QLineEdit::editingFinished, this, [this, source, text] {
            const auto selected = bridge_.application().structural_selection().task_id();
            if (refreshing_ || !editing_enabled() || !selected.has_value())
                return;
            edits_.apply(text, [this, source, selected](auto& draft, auto&, auto&) {
                const auto index = task_index(draft, *selected);
                if (!index.has_value())
                    return;
                auto task = draft.tasks()[*index];
                if (source == task_name_)
                    draft.set_task_name(*index, source->text().toStdString());
                else {
                    if (source == task_priority_) {
                        bool okay = false;
                        const auto value = source->text().toInt(&okay);
                        if (!okay)
                            return;
                        task.priority = value;
                    } else {
                        const auto value = tick_value(source);
                        if (!value.has_value())
                            return;
                        if (source == task_period_)
                            task.period = *value;
                        else if (source == task_deadline_)
                            task.deadline = *value;
                        else
                            task.offset = *value;
                    }
                    draft.set_task_timing(*index, {task.period, task.deadline, task.offset},
                                          task.priority);
                }
            });
        });
    };
    task_text_edit(task_name_, "Rename task");
    task_text_edit(task_period_, "Change task period");
    task_text_edit(task_deadline_, "Change task deadline");
    task_text_edit(task_offset_, "Change task offset");
    task_text_edit(task_priority_, "Change task priority");
    connect(task_id_, &QLineEdit::editingFinished, this, [this] {
        const auto selected = bridge_.application().structural_selection().task_id();
        const auto value = unsigned_value(task_id_);
        if (refreshing_ || !editing_enabled() || !selected.has_value() || !value.has_value() ||
            edit_policy() != ProjectSystemEditPolicy::Generic)
            return;
        const TaskId replacement{*value};
        edits_.apply("Change task ID",
                      [selected, replacement](auto& draft, auto& assignments, auto& selection) {
                          if (const auto index = task_index(draft, *selected); index.has_value()) {
                              draft.set_task_id(*index, replacement);
                              for (auto& assignment : assignments)
                                  if (assignment.task_id == *selected)
                                      assignment.task_id = replacement;
                              selection.select_task(replacement);
                          }
                      });
    });
    connect(task_assignment_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto task_id = bridge_.application().structural_selection().task_id();
        if (refreshing_ || !editing_enabled() || !task_id.has_value() || index < 0) {
            return;
        }
        const auto resource = index == 0 ? std::nullopt
                                         : std::optional<ResourceId>{ResourceId{
                                               task_assignment_->currentData().toULongLong()}};
        static_cast<void>(edits_.apply(
            "Change task assignment", [task_id, resource](auto&, auto& assignments, auto&) {
                const auto found =
                    std::find_if(assignments.begin(), assignments.end(),
                                 [task_id](const auto& row) { return row.task_id == *task_id; });
                if (found != assignments.end()) {
                    found->resource_id = resource;
                } else {
                    assignments.push_back({*task_id, resource});
                }
            }));
    });

    const auto route_edit = [this](QLineEdit* source, const QString& text) {
        connect(source, &QLineEdit::editingFinished, this, [this, source, text] {
            const auto key = selected_route(bridge_.application().structural_selection());
            const auto value = tick_value(source);
            if (refreshing_ || !editing_enabled() || !key.has_value() || !value.has_value())
                return;
            edits_.apply(text, [key, value, source, this](auto& draft, auto&, auto&) {
                if (const auto index = route_index(draft, *key); index.has_value()) {
                    auto route = draft.routes()[*index];
                    if (source == route_send_offset_)
                        route.send_offset = *value;
                    else
                        route.delay = *value;
                    draft.set_message_route(*index, route);
                }
            });
        });
    };
    route_edit(route_send_offset_, "Change send offset");
    route_edit(route_delay_, "Change route delay");
    const auto endpoint_edit = [this](QComboBox* source, bool source_endpoint) {
        connect(
            source, &QComboBox::currentIndexChanged, this, [this, source, source_endpoint](int) {
                const auto key = selected_route(bridge_.application().structural_selection());
                if (refreshing_ || !editing_enabled() || !key.has_value() ||
                    edit_policy() != ProjectSystemEditPolicy::Generic)
                    return;
                const TaskId replacement{source->currentData().toULongLong()};
                edits_.apply("Change route endpoint", [key, replacement, source_endpoint](
                                                           auto& draft, auto&, auto& selection) {
                    if (const auto index = route_index(draft, *key); index.has_value()) {
                        auto route = draft.routes()[*index];
                        if (source_endpoint)
                            route.source_task_id = replacement;
                        else
                            route.destination_task_id = replacement;
                        draft.set_message_route(*index, route);
                        selection.select_message_route(
                            {route.source_task_id, route.destination_task_id});
                    }
                });
            });
    };
    endpoint_edit(connection_source_, true);
    endpoint_edit(connection_destination_, false);

    connect(profile_button_, &QPushButton::clicked, this,
            &QtSystemBuilderWidget::open_execution_profile_dialog);
}

bool QtSystemBuilderWidget::create_component(StructuralSection section) {
    const bool ok = edits_.create_component(section);
    if (ok && section == StructuralSection::Tasks) {
        if (const auto task_id =
                bridge_.application().structural_selection().task_id();
            task_id.has_value()) {
            Q_EMIT taskCreated(*task_id);
        }
    }
    return ok;
}

bool QtSystemBuilderWidget::delete_selected(bool confirmed) {
    if (!confirmed)
        return false;
    return edits_.delete_selected();
}

bool QtSystemBuilderWidget::duplicate_selected() {
    return edits_.duplicate_selected();
}

void QtSystemBuilderWidget::open_execution_profile_dialog() {
    const auto task_id = bridge_.application().structural_selection().task_id();
    if (!task_id.has_value() || !editing_enabled()) {
        return;
    }

    const auto& draft = *bridge_.application().editable_system();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Execution Profiles"));
    dialog.setModal(true);
    dialog.resize(680, 360);

    auto* layout = new QVBoxLayout(&dialog);

    auto* table = new QTableView(&dialog);

    auto* dialog_model = new QtTaskExecutionProfileEditModel(&dialog);
    dialog_model->load(*task_id, draft);

    table->setModel(dialog_model);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addWidget(table);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto* done_button = buttons->addButton(QStringLiteral("Done"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    connect(done_button, &QPushButton::clicked, &dialog, [this, task_id, dialog_model, &dialog]() {
        if (!dialog_model->complete()) {
            QMessageBox::warning(
                &dialog, QStringLiteral("Incomplete Execution Profile"),
                QStringLiteral("Every accessible resource must have a valid WCET."));
            return;
        }

        const auto& rows = dialog_model->rows();

        const bool changed = edits_.apply(
            QStringLiteral("Edit task execution profiles"),
            [task_id, &rows](auto& system_draft, auto&, auto&) {
                for (const auto& row : rows) {
                    if (row.accessible) {
                        system_draft.set_execution_profile(*task_id, row.resource_id,
                                                           *row.execution_time);
                    } else {
                        static_cast<void>(system_draft.remove_execution_profile(
                            DraftExecutionProfileKey{*task_id, row.resource_id}));
                    }
                }
            });

        if (changed) {
            dialog.accept();
        }
    });

    dialog.exec();

    refresh_task_page(*task_id);
}

bool QtSystemBuilderWidget::task_profiles_complete(TaskId task_id) const {
    const auto& draft = *bridge_.application().editable_system();

    for (const auto& resource : draft.resources()) {
        const auto profile = draft.execution_profile(task_id, resource.id);
        if (!profile.has_value() || *profile <= 0) {
            return false;
        }
    }
    return true;
}

void QtSystemBuilderWidget::refresh_profile_button_state(
    TaskId task_id) {

    const bool complete =
        task_profiles_complete(task_id);

    profile_button_->setText(
        complete
            ? QStringLiteral(
                  "Edit Execution Profile")
            : QStringLiteral(
                  "Edit Execution Profile — Incomplete"));

    profile_button_->setToolTip(
        complete
            ? QStringLiteral(
                  "Review or modify the execution times "
                  "for this task.")
            : QStringLiteral(
                  "The execution profile is incomplete. "
                  "Every accessible resource requires a "
                  "valid execution time."));
}

bool QtSystemBuilderWidget::editing_enabled() const {
    return edits_.editing_enabled();
}

ProjectSystemEditPolicy QtSystemBuilderWidget::edit_policy() const {
    return edits_.edit_policy();
}

void QtSystemBuilderWidget::refresh() {
    refreshing_ = true;
    const auto& application = bridge_.application();
    edits_.synchronize_active_project();
    setEnabled(application.editable_system().has_value());
    pages_->setEnabled(editing_enabled());
    if (!application.editable_system().has_value()) {
        pages_->setCurrentWidget(empty_page_);
        empty_message_->setText("No editable project is active.");
    } else {
        const auto& selection = application.structural_selection();
        switch (selection.kind()) {
        case StructuralSelectionKind::System:
            refresh_system_page();
            break;
        case StructuralSelectionKind::Resource:
            refresh_resource_page(*selection.resource_id());
            break;
        case StructuralSelectionKind::Task:
            refresh_task_page(*selection.task_id());
            break;
        case StructuralSelectionKind::ExecutionProfile: {
            const auto profile = *selection.execution_profile();
            refresh_task_page(profile.task_id, profile);
            break;
        }
        case StructuralSelectionKind::MessageRoute:
        case StructuralSelectionKind::Connection:
            refresh_connection_page();
            break;
        default:
            pages_->setCurrentWidget(empty_page_);
            empty_message_->setText("Select an entity to edit its structural properties.");
            break;
        }
    }
    refresh_diagnostics();
    setWindowModified(application.system_changes_dirty());
    refreshing_ = false;
}

void QtSystemBuilderWidget::refresh_system_page() {
    const auto& draft = *bridge_.application().editable_system();
    pages_->setCurrentWidget(system_page_);
    tick_period_->setText(QString::number(draft.tick_period_ns()));
    preemption_->setCurrentIndex(draft.preemption_mode() == PreemptionMode::Preemptive ? 0 : 1);
}

void QtSystemBuilderWidget::refresh_resource_page(ResourceId id) {
    const auto& draft = *bridge_.application().editable_system();
    const auto index = resource_index(draft, id);
    if (!index.has_value()) {
        pages_->setCurrentWidget(empty_page_);
        return;
    }
    pages_->setCurrentWidget(resource_page_);
    resource_id_->setText(QString::number(id.value()));
    resource_name_->setText(QString::fromStdString(draft.resources()[*index].name));
    resource_id_->setEnabled(editing_enabled());
    resource_name_->setEnabled(editing_enabled());
}

void QtSystemBuilderWidget::refresh_task_page(TaskId id,
                                              std::optional<DraftExecutionProfileKey> profile) {
    const auto& draft = *bridge_.application().editable_system();
    const auto index = task_index(draft, id);
    if (!index.has_value()) {
        pages_->setCurrentWidget(empty_page_);
        return;
    }
    const auto& task = draft.tasks()[*index];
    pages_->setCurrentWidget(task_page_);
    task_id_->setText(QString::number(task.id.value()));
    task_name_->setText(QString::fromStdString(task.name));
    task_period_->setText(QString::number(task.period));
    task_deadline_->setText(QString::number(task.deadline));
    task_offset_->setText(QString::number(task.offset));
    task_priority_->setText(QString::number(task.priority));
    const auto protected_task = edit_policy() == ProjectSystemEditPolicy::BoschCompatible;
    task_id_->setEnabled(editing_enabled() && !protected_task);
    task_name_->setEnabled(editing_enabled() && !protected_task);
    for (auto* edit : {task_period_, task_deadline_, task_offset_, task_priority_})
        edit->setEnabled(editing_enabled());
    protected_help_->setText(
        protected_task ? "Bosch task IDs and names are adapter-owned. Timing and priority "
                         "remain editable while paused."
                       : QString{});
    static_cast<void>(profile);
    task_assignment_->clear();
    task_assignment_->addItem(QStringLiteral("Unassigned"));
    for (const auto& resource : draft.resources()) {
        task_assignment_->addItem(QString::fromStdString(resource.name),
                                  static_cast<qulonglong>(resource.id.value()));
    }
    const auto assignment = std::find_if(bridge_.application().run_assignments().begin(),
                                         bridge_.application().run_assignments().end(),
                                         [id](const auto& row) { return row.task_id == id; });
    const auto assigned = assignment != bridge_.application().run_assignments().end()
                              ? assignment->resource_id
                              : std::nullopt;
    task_assignment_->setCurrentIndex(
        assigned.has_value()
            ? task_assignment_->findData(static_cast<qulonglong>(assigned->value()))
            : 0);
    task_assignment_->setEnabled(editing_enabled());
    const auto wcet = assigned.has_value() ? draft.execution_profile(id, *assigned) : std::nullopt;
    assignment_status_->setText(!assigned.has_value() ? QStringLiteral("Unassigned")
                                : wcet.has_value()    ? QStringLiteral("Valid")
                                                      : QStringLiteral("Missing WCET profile"));
    refresh_profile_button_state(id);
}

void QtSystemBuilderWidget::refresh_connection_page() {
    const auto& application = bridge_.application();
    const auto& draft = *application.editable_system();
    const auto key = selected_route(application.structural_selection());
    pages_->setCurrentWidget(connection_page_);
    connection_source_->clear();
    connection_destination_->clear();
    for (const auto& task : draft.tasks()) {
        const auto value = static_cast<qulonglong>(task.id.value());
        connection_source_->addItem(QString::fromStdString(task.name), value);
        connection_destination_->addItem(QString::fromStdString(task.name), value);
    }
    if (key.has_value()) {
        connection_source_->setCurrentIndex(
            connection_source_->findData(static_cast<qulonglong>(key->source_task_id.value())));
        connection_destination_->setCurrentIndex(connection_destination_->findData(
            static_cast<qulonglong>(key->destination_task_id.value())));
    }
    const auto index = key.has_value() ? route_index(draft, *key) : std::nullopt;
    const auto logical =
        application.structural_selection().connection().has_value() &&
        application.structural_selection().connection()->kind == GuiConnectionKind::Logical;
    connection_kind_->setText(logical ? "Logical" : "Communication");
    const auto bosch = edit_policy() == ProjectSystemEditPolicy::BoschCompatible;
    const auto latency = logical             ? Tick{0}
                         : bosch             ? Tick{80}
                         : index.has_value() ? draft.routes()[*index].delay
                                             : Tick{0};
    connection_latency_->setText(QString{"%1 ticks"}.arg(latency));
    route_send_offset_->setVisible(index.has_value());
    route_delay_->setVisible(index.has_value());
    if (index.has_value()) {
        route_send_offset_->setText(QString::number(draft.routes()[*index].send_offset));
        route_delay_->setText(QString::number(draft.routes()[*index].delay));
    }
    connection_source_->setEnabled(editing_enabled() && index.has_value() && !bosch);
    connection_destination_->setEnabled(editing_enabled() && index.has_value() && !bosch);
    route_send_offset_->setEnabled(editing_enabled() && index.has_value());
    route_delay_->setEnabled(editing_enabled() && index.has_value());
}

void QtSystemBuilderWidget::refresh_diagnostics() {
    const auto& application = bridge_.application();
    for (auto* label :
         {system_diagnostic_, resource_diagnostic_, task_diagnostic_, connection_diagnostic_}) {
        label->clear();
    }
    if (application.run_state() == GuiRunState::Running) {
        diagnostic_summary_->setText("Pause the simulation to edit the system.");
        return;
    }
    const auto& diagnostics = application.system_validation().diagnostics;
    if (diagnostics.empty()) {
        diagnostic_summary_->setText("Validation: no structural issues.");
    } else {
        diagnostic_summary_->setText(QString{"Validation: %1 issue(s)\n%2"}
                                         .arg(diagnostics.size())
                                         .arg(QString::fromStdString(diagnostics.front().message)));
        const auto& selection = application.structural_selection();
        for (const auto& diagnostic : diagnostics) {
            QLabel* field = nullptr;
            if (diagnostic.entity_kind == SystemDraftEntityKind::System) {
                field = system_diagnostic_;
            } else if (diagnostic.entity_kind == SystemDraftEntityKind::Resource &&
                       selection.resource_id() == diagnostic.resource_id) {
                field = resource_diagnostic_;
            } else if ((diagnostic.entity_kind == SystemDraftEntityKind::Task ||
                        diagnostic.entity_kind == SystemDraftEntityKind::ExecutionProfile) &&
                       selection.task_id() == diagnostic.task_id) {
                field = task_diagnostic_;
            } else if (diagnostic.entity_kind == SystemDraftEntityKind::MessageRoute &&
                       selected_route(selection).has_value()) {
                field = connection_diagnostic_;
            }
            if (field != nullptr && field->text().isEmpty()) {
                field->setText(QString::fromStdString(diagnostic.message));
            }
        }
    }
}

} // namespace cpssim::qt
