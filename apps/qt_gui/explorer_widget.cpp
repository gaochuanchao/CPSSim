/*** Present structural entities without owning or mutating domain truth. ***/
#include "apps/qt_gui/explorer_widget.hpp"

#include "apps/qt_gui/system_builder_widget.hpp"

#include <QAction>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <optional>

namespace cpssim::qt {
namespace {

enum ItemRole {
    KindRole = Qt::UserRole + 1,
    PrimaryIdRole,
    SecondaryIdRole,
    SectionRole,
};

enum class ExplorerItemKind { System, Section, Resource, Task, Route };

QStandardItem* item(QString text, ExplorerItemKind kind) {
    auto* result = new QStandardItem(std::move(text));
    result->setEditable(false);
    result->setData(static_cast<int>(kind), KindRole);
    return result;
}

QStandardItem* section_item(const QString& text, StructuralSection section) {
    auto* result = item(text, ExplorerItemKind::Section);
    result->setData(static_cast<int>(section), SectionRole);
    return result;
}

bool item_matches(const QStandardItem& item_value, const StructuralSelection& selection) {
    const auto kind = static_cast<ExplorerItemKind>(item_value.data(KindRole).toInt());
    const auto first = item_value.data(PrimaryIdRole).toULongLong();
    const auto second = item_value.data(SecondaryIdRole).toULongLong();
    switch (kind) {
    case ExplorerItemKind::System:
        return selection.kind() == StructuralSelectionKind::System;
    case ExplorerItemKind::Section:
        return selection.section().has_value() &&
               static_cast<int>(*selection.section()) == item_value.data(SectionRole).toInt();
    case ExplorerItemKind::Resource:
        return selection.resource_id() == ResourceId{first};
    case ExplorerItemKind::Task:
        return selection.task_id() == TaskId{first} &&
               selection.kind() == StructuralSelectionKind::Task;
    case ExplorerItemKind::Route:
        return selection.message_route() ==
               std::optional<DraftMessageRouteKey>{{TaskId{first}, TaskId{second}}};
    }
    return false;
}

} // namespace

QtExperimentExplorerWidget::QtExperimentExplorerWidget(QtWorkbenchBridge& bridge,
                                                       QtSystemBuilderWidget& builder,
                                                       QWidget* parent)
    : QWidget(parent), bridge_{bridge}, builder_{builder}, tree_{new QTreeView(this)},
      model_{new QStandardItemModel(this)} {
    setObjectName("view.explorer");
    model_->setHorizontalHeaderLabels({"Experiment"});
    tree_->setObjectName("explorer.tree");
    tree_->setModel(model_);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->header()->setStretchLastSection(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(tree_);
    connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { apply_current_selection(); });
    connect(tree_, &QTreeView::customContextMenuRequested, this,
            &QtExperimentExplorerWidget::show_context_menu);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtExperimentExplorerWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtExperimentExplorerWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtExperimentExplorerWidget::synchronize_selection);
    refresh();
}

void QtExperimentExplorerWidget::refresh() {
    refreshing_ = true;
    model_->removeRows(0, model_->rowCount());
    auto* root =
        item(bridge_.application().has_active_project()
                 ? QString::fromStdString(bridge_.application().active_project().metadata().name)
                 : QStringLiteral("System"),
             ExplorerItemKind::System);
    model_->appendRow(root);
    if (bridge_.application().editable_system().has_value()) {
        const auto& draft = *bridge_.application().editable_system();
        auto* resources = section_item("Resources", StructuralSection::Resources);
        for (const auto& resource : draft.resources()) {
            auto* row = item(QString{"%1 (R%2)"}
                                 .arg(QString::fromStdString(resource.name))
                                 .arg(resource.id.value()),
                             ExplorerItemKind::Resource);
            row->setData(static_cast<qulonglong>(resource.id.value()), PrimaryIdRole);
            resources->appendRow(row);
        }
        auto* tasks = section_item("Tasks", StructuralSection::Tasks);
        for (const auto& task : draft.tasks()) {
            auto* row = item(
                QString{"%1 (T%2)"}.arg(QString::fromStdString(task.name)).arg(task.id.value()),
                ExplorerItemKind::Task);
            row->setData(static_cast<qulonglong>(task.id.value()), PrimaryIdRole);
            tasks->appendRow(row);
        }
        auto* routes = section_item("Connections", StructuralSection::MessageRoutes);
        for (const auto& route : draft.routes()) {
            auto* row = item(QString{"T%1 → T%2"}
                                 .arg(route.source_task_id.value())
                                 .arg(route.destination_task_id.value()),
                             ExplorerItemKind::Route);
            row->setData(static_cast<qulonglong>(route.source_task_id.value()), PrimaryIdRole);
            row->setData(static_cast<qulonglong>(route.destination_task_id.value()),
                         SecondaryIdRole);
            routes->appendRow(row);
        }
        root->appendRow(resources);
        root->appendRow(tasks);
        root->appendRow(routes);
    }
    tree_->expandToDepth(1);
    refreshing_ = false;
    synchronize_selection();
}

void QtExperimentExplorerWidget::apply_current_selection() {
    if (refreshing_ || !tree_->currentIndex().isValid()) {
        return;
    }
    const auto* selected = model_->itemFromIndex(tree_->currentIndex());
    const auto kind = static_cast<ExplorerItemKind>(selected->data(KindRole).toInt());
    const auto first = selected->data(PrimaryIdRole).toULongLong();
    const auto second = selected->data(SecondaryIdRole).toULongLong();
    auto& selection = bridge_.application().structural_selection();
    switch (kind) {
    case ExplorerItemKind::System:
        selection.select_system();
        break;
    case ExplorerItemKind::Section:
        selection.select_section(
            static_cast<StructuralSection>(selected->data(SectionRole).toInt()));
        break;
    case ExplorerItemKind::Resource:
        selection.select_resource(ResourceId{first});
        break;
    case ExplorerItemKind::Task:
        selection.select_task(TaskId{first});
        break;
    case ExplorerItemKind::Route:
        selection.select_message_route({TaskId{first}, TaskId{second}});
        break;
    }
    bridge_.notify_structural_selection_changed();
}

void QtExperimentExplorerWidget::show_context_menu(const QPoint& position) {
    const auto index = tree_->indexAt(position);
    if (!index.isValid()) {
        return;
    }
    tree_->setCurrentIndex(index);
    const auto* selected = model_->itemFromIndex(index);
    const auto kind = static_cast<ExplorerItemKind>(selected->data(KindRole).toInt());
    QMenu menu(this);
    if (kind == ExplorerItemKind::Section) {
        const auto section = static_cast<StructuralSection>(selected->data(SectionRole).toInt());
        QAction* add = nullptr;
        switch (section) {
        case StructuralSection::Resources:
            add = menu.addAction("Add Resource");
            break;
        case StructuralSection::Tasks:
            add = menu.addAction("Add Task");
            break;
        case StructuralSection::MessageRoutes:
            add = menu.addAction("Add Communication Connection");
            break;
        default:
            break;
        }
        if (section == StructuralSection::MessageRoutes) {
            auto* logical = menu.addAction("Add Logical Connection");
            logical->setEnabled(false);
            logical->setToolTip("Logical dependencies are adapter-owned and cannot yet be "
                                "persisted authoritatively.");
        }
        if (add != nullptr) {
            connect(add, &QAction::triggered, this,
                    [this, section] { static_cast<void>(builder_.create_component(section)); });
        }
    } else if (kind != ExplorerItemKind::System) {
        auto* duplicate = menu.addAction("Duplicate");
        auto* remove = menu.addAction("Delete...");
        connect(duplicate, &QAction::triggered, this,
                [this] { static_cast<void>(builder_.duplicate_selected()); });
        connect(remove, &QAction::triggered, this, [this] {
            const auto answer = QMessageBox::question(
                this, "Delete structural entity",
                "Delete the selected entity and its dependent draft references?");
            if (answer == QMessageBox::Yes) {
                static_cast<void>(builder_.delete_selected(true));
            }
        });
    }
    if (!menu.isEmpty()) {
        menu.exec(tree_->viewport()->mapToGlobal(position));
    }
}

void QtExperimentExplorerWidget::synchronize_selection() {
    const auto selected = bridge_.application().structural_selection();
    const auto root = model_->invisibleRootItem();
    QList<QStandardItem*> pending;
    for (int row = 0; row < root->rowCount(); ++row) {
        pending.push_back(root->child(row));
    }
    while (!pending.empty()) {
        auto* candidate = pending.takeFirst();
        if (item_matches(*candidate, selected)) {
            const QSignalBlocker blocker{tree_->selectionModel()};
            tree_->setCurrentIndex(candidate->index());
            tree_->scrollTo(candidate->index());
            return;
        }
        for (int row = 0; row < candidate->rowCount(); ++row) {
            pending.push_back(candidate->child(row));
        }
    }
}

} // namespace cpssim::qt
