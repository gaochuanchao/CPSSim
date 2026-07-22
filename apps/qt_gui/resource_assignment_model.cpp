/*** Implement the typed Resource Assignments dock model and delegate. ***/
#include "apps/qt_gui/resource_assignment_model.hpp"

#include "apps/qt_gui/workbench_style.hpp"

#include <QComboBox>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <utility>

namespace cpssim::qt {
namespace {

QIcon color_icon(const QColor& color) {
    QPixmap image{12, 12};
    image.fill(Qt::transparent);
    QPainter painter{&image};
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(image.rect(), 2, 2);
    return QIcon{image};
}

qulonglong resource_value(ResourceId id) { return static_cast<qulonglong>(id.value()); }

} // namespace

QtResourceAssignmentModel::QtResourceAssignmentModel(QtWorkbenchBridge& bridge, QObject* parent)
    : QAbstractTableModel(parent), bridge_{bridge} {
    rebuild();
}

int QtResourceAssignmentModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int QtResourceAssignmentModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : Count;
}

QVariant QtResourceAssignmentModel::data(const QModelIndex& index, int role) const {
    const auto* row = row_at(index.row());
    if (row == nullptr || !index.isValid()) {
        return {};
    }
    if (role == TaskIdRole) {
        return QVariant::fromValue(static_cast<qulonglong>(row->task_id.value()));
    }
    if (role == ResourceIdRole) {
        return row->resource_id.has_value() ? QVariant::fromValue(resource_value(*row->resource_id))
                                            : QVariant{};
    }
    if (role == Qt::DecorationRole && index.column() == Swatch) {
        const auto theme = current_workbench_theme();
        return color_icon(row->resource_id.has_value()
                              ? resource_accent_color(*row->resource_id, theme)
                              : unassigned_accent_color(theme));
    }
    if (role == Qt::ToolTipRole) {
        return row->status;
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }
    switch (index.column()) {
    case Swatch:
        return {};
    case Task:
        return row->task_name;
    case Resource:
        return row->resource_name;
    case Profile:
        return row->execution_time.has_value() ? QString{"%1 ticks"}.arg(*row->execution_time)
                                               : QStringLiteral("—");
    case PriorityColumn:
        return row->priority;
    case Accessibility:
        return row->accessible ? QStringLiteral("Accessible") : QStringLiteral("No profile");
    case Status:
        return row->status;
    default:
        return {};
    }
}

QVariant QtResourceAssignmentModel::headerData(int section, Qt::Orientation orientation,
                                               int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case Swatch:
        return QString{};
    case Task:
        return QStringLiteral("Task");
    case Resource:
        return QStringLiteral("Resource");
    case Profile:
        return QStringLiteral("Profile/WCET");
    case PriorityColumn:
        return QStringLiteral("Priority");
    case Accessibility:
        return QStringLiteral("Accessibility");
    case Status:
        return QStringLiteral("Status");
    default:
        return {};
    }
}

Qt::ItemFlags QtResourceAssignmentModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == Resource &&
        bridge_.application().run_state() != GuiRunState::Running &&
        bridge_.application().editable_system().has_value()) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool QtResourceAssignmentModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    const auto* row = row_at(index.row());
    if (row == nullptr || index.column() != Resource || role != Qt::EditRole) {
        return false;
    }
    const auto resource =
        value.isValid() ? std::optional<ResourceId>{ResourceId{value.toULongLong()}} : std::nullopt;
    if (!bridge_.assign_task(row->task_id, resource)) {
        return false;
    }
    rebuild();
    return true;
}

void QtResourceAssignmentModel::sort(int column, Qt::SortOrder order) {
    if (column != Task && column != Resource && column != PriorityColumn && column != Status) {
        return;
    }
    sort_column_ = column;
    sort_order_ = order;
    beginResetModel();
    const auto less = [column](const auto& left, const auto& right) {
        switch (column) {
        case Resource:
            return left.resource_name < right.resource_name;
        case PriorityColumn:
            return left.priority < right.priority;
        case Status:
            return left.status < right.status;
        default:
            return left.task_name < right.task_name;
        }
    };
    std::stable_sort(rows_.begin(), rows_.end(), [&](const auto& left, const auto& right) {
        return order == Qt::AscendingOrder ? less(left, right) : less(right, left);
    });
    endResetModel();
}

void QtResourceAssignmentModel::rebuild() {
    beginResetModel();
    rows_.clear();
    const auto& application = bridge_.application();
    if (application.editable_system().has_value()) {
        const auto& draft = *application.editable_system();
        for (const auto& task : draft.tasks()) {
            QtResourceAssignmentRow row{.task_id = task.id,
                                        .task_name = QString::fromStdString(task.name),
                                        .resource_id = std::nullopt,
                                        .resource_name = QStringLiteral("Unassigned"),
                                        .execution_time = std::nullopt,
                                        .priority = task.priority,
                                        .accessible = false,
                                        .status = QStringLiteral("Unassigned")};
            const auto assignment = std::find_if(
                application.run_assignments().begin(), application.run_assignments().end(),
                [&](const auto& value) { return value.task_id == task.id; });
            if (assignment != application.run_assignments().end() &&
                assignment->resource_id.has_value()) {
                row.resource_id = assignment->resource_id;
                const auto resource =
                    std::find_if(draft.resources().begin(), draft.resources().end(),
                                 [&](const auto& value) { return value.id == *row.resource_id; });
                row.resource_name = resource == draft.resources().end()
                                        ? QStringLiteral("Missing resource")
                                        : QString::fromStdString(resource->name);
                row.execution_time = draft.execution_profile(task.id, *row.resource_id);
                row.accessible = row.execution_time.has_value();
                row.status = row.accessible ? QStringLiteral("Valid")
                                            : QStringLiteral("No execution profile");
            }
            rows_.push_back(std::move(row));
        }
    }
    endResetModel();
    sort(sort_column_, sort_order_);
}

const QtResourceAssignmentRow* QtResourceAssignmentModel::row_at(int row) const {
    return row >= 0 && row < static_cast<int>(rows_.size()) ? &rows_[static_cast<std::size_t>(row)]
                                                            : nullptr;
}

const std::vector<DraftResource>& QtResourceAssignmentModel::resources() const {
    static const std::vector<DraftResource> empty;
    const auto& draft = bridge_.application().editable_system();
    return draft.has_value() ? draft->resources() : empty;
}

QtResourceAssignmentDelegate::QtResourceAssignmentDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QWidget* QtResourceAssignmentDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                                    const QModelIndex& index) const {
    const auto* assignment_model = dynamic_cast<const QtResourceAssignmentModel*>(index.model());
    if (assignment_model == nullptr || index.column() != QtResourceAssignmentModel::Resource) {
        return nullptr;
    }
    auto* combo = new QComboBox(parent);
    combo->addItem(QStringLiteral("Unassigned"), QVariant{});
    for (const auto& resource : assignment_model->resources()) {
        combo->addItem(QString::fromStdString(resource.name), resource_value(resource.id));
    }
    return combo;
}

void QtResourceAssignmentDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);
    if (combo == nullptr) {
        return;
    }
    const auto current = index.data(QtResourceAssignmentModel::ResourceIdRole);
    combo->setCurrentIndex(current.isValid() ? combo->findData(current) : 0);
}

void QtResourceAssignmentDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                                const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);
    if (combo != nullptr) {
        static_cast<void>(model->setData(index, combo->currentData(), Qt::EditRole));
    }
}

QtResourceAssignmentsWidget::QtResourceAssignmentsWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, model_{bridge, this} {
    setObjectName("view.resourceAssignments");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* legend_widget = new QWidget(this);
    legend_ = new QHBoxLayout(legend_widget);
    legend_->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(legend_widget);
    table_ = new QTableView(this);
    table_->setObjectName("table.resourceAssignments");
    table_->setModel(&model_);
    table_->setItemDelegateForColumn(QtResourceAssignmentModel::Resource,
                                     new QtResourceAssignmentDelegate(table_));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setSortingEnabled(true);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(QtResourceAssignmentModel::Status,
                                                     QHeaderView::Stretch);
    layout->addWidget(table_);

    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this,
            &QtResourceAssignmentsWidget::rebuild);
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtResourceAssignmentsWidget::rebuild);
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtResourceAssignmentsWidget::select_table_row);
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) {
                if (const auto* row = model_.row_at(current.row()); row != nullptr) {
                    bridge_.application().structural_selection().select_task(row->task_id);
                    bridge_.notify_structural_selection_changed();
                }
            });
    rebuild_legend();
}

void QtResourceAssignmentsWidget::rebuild() {
    model_.rebuild();
    rebuild_legend();
    select_table_row();
}

void QtResourceAssignmentsWidget::rebuild_legend() {
    while (auto* item = legend_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    std::map<ResourceId, std::pair<QString, int>> counts;
    int unassigned = 0;
    for (int row_index = 0; row_index < model_.rowCount(); ++row_index) {
        const auto* row = model_.row_at(row_index);
        if (row->resource_id.has_value()) {
            auto& entry = counts[*row->resource_id];
            entry.first = row->resource_name;
            ++entry.second;
        } else {
            ++unassigned;
        }
    }
    const auto theme = current_workbench_theme();
    for (const auto& [resource_id, entry] : counts) {
        auto* button = new QToolButton(this);
        button->setIcon(color_icon(resource_accent_color(resource_id, theme)));
        button->setText(QString{"%1 · %2 tasks"}.arg(entry.first).arg(entry.second));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(button, &QToolButton::clicked, this,
                [this, resource_id] { bridge_.set_resource_highlight(resource_id); });
        legend_->addWidget(button);
    }
    auto* unassigned_label = new QLabel(QString{"□ Unassigned · %1 tasks"}.arg(unassigned), this);
    legend_->addWidget(unassigned_label);
    legend_->addStretch();
}

void QtResourceAssignmentsWidget::select_table_row() {
    const auto task = bridge_.application().structural_selection().task_id();
    if (!task.has_value()) {
        return;
    }
    for (int row = 0; row < model_.rowCount(); ++row) {
        const auto* value = model_.row_at(row);
        if (value != nullptr && value->task_id == *task) {
            const QSignalBlocker blocker{table_->selectionModel()};
            table_->selectRow(row);
            table_->scrollTo(model_.index(row, QtResourceAssignmentModel::Task));
            return;
        }
    }
}

} // namespace cpssim::qt
