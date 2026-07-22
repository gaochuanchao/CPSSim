/*** Implement domain-backed Qt System Builder editors and undo commands. ***/
#include "apps/qt_gui/system_builder_widget.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace cpssim::qt {
namespace {

struct DraftState {
    EditableSystemDraft draft;
    std::vector<DraftTaskAssignment> assignments;
    StructuralSelection selection;
};

class RestoreDraftCommand final : public QUndoCommand {
  public:
    RestoreDraftCommand(QtWorkbenchBridge& bridge, DraftState before, DraftState after,
                        const QString& text)
        : QUndoCommand(text), bridge_{bridge}, before_{std::move(before)},
          after_{std::move(after)} {}

    void undo() override { restore(before_); }
    void redo() override { restore(after_); }

  private:
    void restore(const DraftState& state) {
        bridge_.restore_draft(state.draft, state.assignments, state.selection);
    }

    QtWorkbenchBridge& bridge_;
    DraftState before_;
    DraftState after_;
};

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

QtSystemBuilderWidget::QtSystemBuilderWidget(QtWorkbenchBridge& bridge, QWidget* parent)
    : QWidget(parent), bridge_{bridge}, undo_stack_{new QUndoStack(this)} {
    setObjectName("view.systemBuilder");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName("splitter.systemBuilder");

    auto* editor = new QWidget(splitter);
    auto* editor_layout = new QVBoxLayout(editor);
    pages_ = new QStackedWidget(editor);
    pages_->setObjectName("systemBuilder.pages");
    build_pages();
    editor_layout->addWidget(pages_);
    diagnostic_summary_ = new QLabel(editor);
    diagnostic_summary_->setObjectName("systemBuilder.diagnostics");
    diagnostic_summary_->setWordWrap(true);
    editor_layout->addWidget(diagnostic_summary_);

    auto* library = new QWidget(splitter);
    auto* library_layout = new QVBoxLayout(library);
    library_layout->addWidget(new QLabel("Component Library", library));
    component_library_ = new QListWidget(library);
    component_library_->setObjectName("systemBuilder.componentLibrary");
    component_library_->addItems({"Resource", "Task", "Execution Profile", "Message Route"});
    component_library_->setCurrentRow(0);
    library_layout->addWidget(component_library_);
    add_component_ = new QPushButton("Add Selected Component", library);
    add_component_->setObjectName("systemBuilder.addComponent");
    delete_component_ = new QPushButton("Delete Selected Entity...", library);
    delete_component_->setObjectName("systemBuilder.deleteComponent");
    library_layout->addWidget(add_component_);
    library_layout->addWidget(delete_component_);

    splitter->addWidget(editor);
    splitter->addWidget(library);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);

    connect_editors();
    connect(&bridge_, &QtWorkbenchBridge::structuralSelectionChanged, this,
            &QtSystemBuilderWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::draftChanged, this, &QtSystemBuilderWidget::refresh);
    connect(&bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtSystemBuilderWidget::refresh);
    connect(add_component_, &QPushButton::clicked, this, [this] {
        const auto section = static_cast<StructuralSection>(component_library_->currentRow());
        static_cast<void>(create_component(section));
    });
    connect(delete_component_, &QPushButton::clicked, this, [this] {
        const auto response =
            QMessageBox::question(this, "Delete structural entity",
                                  "Delete the selected entity and its dependent draft references?");
        if (response == QMessageBox::Yes) {
            static_cast<void>(delete_selected(true));
        }
    });
    refresh();
}

void QtSystemBuilderWidget::build_pages() {
    QFormLayout* form = nullptr;
    empty_page_ = form_page(pages_, form);
    empty_message_ = new QLabel("Select a system entity in Explorer or Architecture.", empty_page_);
    empty_message_->setWordWrap(true);
    form->addRow(empty_message_);

    system_page_ = form_page(pages_, form);
    tick_period_ = new QLineEdit(system_page_);
    tick_period_->setObjectName("systemBuilder.tickPeriod");
    preemption_ = new QComboBox(system_page_);
    preemption_->setObjectName("systemBuilder.preemption");
    preemption_->addItems({"Preemptive", "Non-preemptive"});
    form->addRow("Tick period (ns)", tick_period_);
    form->addRow("Preemption mode", preemption_);
    system_diagnostic_ = diagnostic_label(system_page_, "systemBuilder.systemDiagnostic");
    form->addRow(system_diagnostic_);

    resource_page_ = form_page(pages_, form);
    resource_id_ = new QLineEdit(resource_page_);
    resource_name_ = new QLineEdit(resource_page_);
    resource_id_->setObjectName("systemBuilder.resourceId");
    resource_name_->setObjectName("systemBuilder.resourceName");
    form->addRow("Resource ID", resource_id_);
    form->addRow("Name", resource_name_);
    resource_diagnostic_ = diagnostic_label(resource_page_, "systemBuilder.resourceDiagnostic");
    form->addRow(resource_diagnostic_);

    task_page_ = form_page(pages_, form);
    task_id_ = new QLineEdit(task_page_);
    task_name_ = new QLineEdit(task_page_);
    task_period_ = new QLineEdit(task_page_);
    task_deadline_ = new QLineEdit(task_page_);
    task_offset_ = new QLineEdit(task_page_);
    task_priority_ = new QLineEdit(task_page_);
    profile_resource_ = new QComboBox(task_page_);
    profile_execution_ = new QLineEdit(task_page_);
    profile_label_ = new QLabel("Execution Profile", task_page_);
    profile_label_->setStyleSheet("font-weight: bold");
    protected_help_ = new QLabel(task_page_);
    protected_help_->setWordWrap(true);
    task_id_->setObjectName("systemBuilder.taskId");
    task_name_->setObjectName("systemBuilder.taskName");
    form->addRow("Task ID", task_id_);
    form->addRow("Name", task_name_);
    form->addRow("Period", task_period_);
    form->addRow("Deadline", task_deadline_);
    form->addRow("Offset", task_offset_);
    form->addRow("Priority", task_priority_);
    form->addRow(profile_label_);
    form->addRow("Resource", profile_resource_);
    form->addRow("Execution time", profile_execution_);
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
    form->addRow("Source", connection_source_);
    form->addRow("Destination", connection_destination_);
    form->addRow("Kind", connection_kind_);
    form->addRow("Displayed latency", connection_latency_);
    form->addRow("Send offset", route_send_offset_);
    form->addRow("Delay", route_delay_);
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
    for (auto* edit :
         {tick_period_, resource_id_, task_id_, task_period_, task_deadline_, task_offset_,
          task_priority_, profile_execution_, route_send_offset_, route_delay_}) {
        edit->setValidator(unsigned_validator);
    }
    connect(tick_period_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_ || !editing_enabled())
            return;
        const auto value = tick_value(tick_period_);
        if (!value.has_value())
            return;
        push_mutation("Change tick period",
                      [value](auto& draft, auto&, auto&) { draft.set_tick_period_ns(*value); });
    });
    connect(preemption_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_ || !editing_enabled())
            return;
        push_mutation("Change preemption mode", [index](auto& draft, auto&, auto&) {
            draft.set_preemption_mode(index == 0 ? PreemptionMode::Preemptive
                                                 : PreemptionMode::NonPreemptive);
        });
    });
    connect(resource_name_, &QLineEdit::editingFinished, this, [this] {
        const auto selected = bridge_.application().structural_selection().resource_id();
        if (refreshing_ || !editing_enabled() || !selected.has_value())
            return;
        const auto name = resource_name_->text().toStdString();
        push_mutation("Rename resource", [selected, name](auto& draft, auto&, auto&) {
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
        push_mutation("Change resource ID", [selected, replacement](auto& draft, auto& assignments,
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
            push_mutation(text, [this, source, selected](auto& draft, auto&, auto&) {
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
        push_mutation("Change task ID",
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
    connect(profile_execution_, &QLineEdit::editingFinished, this, [this] {
        const auto key = bridge_.application().structural_selection().execution_profile();
        const auto value = tick_value(profile_execution_);
        if (refreshing_ || !editing_enabled() || !key.has_value() || !value.has_value())
            return;
        push_mutation("Change execution time", [key, value](auto& draft, auto&, auto&) {
            draft.set_execution_profile(key->task_id, key->resource_id, *value);
        });
    });
    connect(profile_resource_, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto key = bridge_.application().structural_selection().execution_profile();
        if (refreshing_ || !editing_enabled() || !key.has_value())
            return;
        const ResourceId replacement{profile_resource_->currentData().toULongLong()};
        push_mutation(
            "Change profile resource", [key, replacement](auto& draft, auto&, auto& selection) {
                const auto execution = draft.execution_profile(key->task_id, key->resource_id);
                if (execution.has_value() &&
                    !draft.execution_profile(key->task_id, replacement).has_value()) {
                    draft.remove_execution_profile(*key);
                    draft.set_execution_profile(key->task_id, replacement, *execution);
                    selection.select_execution_profile({key->task_id, replacement});
                }
            });
    });

    const auto route_edit = [this](QLineEdit* source, const QString& text) {
        connect(source, &QLineEdit::editingFinished, this, [this, source, text] {
            const auto key = selected_route(bridge_.application().structural_selection());
            const auto value = tick_value(source);
            if (refreshing_ || !editing_enabled() || !key.has_value() || !value.has_value())
                return;
            push_mutation(text, [key, value, source, this](auto& draft, auto&, auto&) {
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
                push_mutation("Change route endpoint", [key, replacement, source_endpoint](
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
}

void QtSystemBuilderWidget::push_mutation(const QString& text, DraftMutator mutator) {
    auto& application = bridge_.application();
    if (!application.editable_system().has_value())
        return;
    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;
    try {
        mutator(after.draft, after.assignments, after.selection);
        undo_stack_->push(
            new RestoreDraftCommand{bridge_, std::move(before), std::move(after), text});
    } catch (const std::exception& error) {
        application.set_status(error.what(), true);
        Q_EMIT bridge_.statusChanged();
    }
}

bool QtSystemBuilderWidget::create_component(StructuralSection section) {
    auto& application = bridge_.application();
    if (!editing_enabled() || !application.editable_system().has_value())
        return false;
    if (section == StructuralSection::Tasks && edit_policy() != ProjectSystemEditPolicy::Generic) {
        application.set_status("Adapter-owned task identities are protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    DraftState before{*application.editable_system(), application.run_assignments(),
                      application.structural_selection()};
    auto after = before;
    SystemExplorerInteraction interaction;
    auto result = interaction.create(section, after.draft, after.selection);
    if (!result.changed) {
        application.set_status(result.diagnostic, true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    for (const auto& task : after.draft.tasks()) {
        const auto exists = std::any_of(after.assignments.begin(), after.assignments.end(),
                                        [&](const auto& row) { return row.task_id == task.id; });
        if (!exists)
            after.assignments.push_back({task.id, std::nullopt});
    }
    undo_stack_->push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                              QStringLiteral("Add component")});
    return true;
}

bool QtSystemBuilderWidget::delete_selected(bool confirmed) {
    if (!confirmed || !editing_enabled())
        return false;
    auto& application = bridge_.application();
    const auto selection = application.structural_selection();
    if ((selection.kind() == StructuralSelectionKind::Task ||
         selection.kind() == StructuralSelectionKind::MessageRoute ||
         selection.kind() == StructuralSelectionKind::Connection) &&
        edit_policy() == ProjectSystemEditPolicy::BoschCompatible) {
        application.set_status("This Bosch structural identity is protected.", true);
        Q_EMIT bridge_.statusChanged();
        return false;
    }
    DraftState before{*application.editable_system(), application.run_assignments(), selection};
    auto after = before;
    SystemExplorerInteraction interaction;
    if (!interaction.request_delete(after.selection, after.draft, after.assignments))
        return false;
    const auto result = interaction.confirm_delete(after.draft, after.assignments, after.selection);
    if (!result.changed)
        return false;
    undo_stack_->push(new RestoreDraftCommand{bridge_, std::move(before), std::move(after),
                                              QStringLiteral("Delete component")});
    return true;
}

bool QtSystemBuilderWidget::editing_enabled() const {
    return bridge_.application().editable_system().has_value() &&
           bridge_.application().run_state() != GuiRunState::Running;
}

ProjectSystemEditPolicy QtSystemBuilderWidget::edit_policy() const {
    const auto& application = bridge_.application();
    return application.has_active_project()
               ? project_system_edit_policy(application.active_project().metadata())
               : ProjectSystemEditPolicy::ReadOnlyAdapter;
}

void QtSystemBuilderWidget::refresh() {
    refreshing_ = true;
    const auto& application = bridge_.application();
    const auto active_root = application.has_active_project()
                                 ? std::optional{application.active_project().root()}
                                 : std::nullopt;
    if (active_root != undo_project_root_) {
        undo_stack_->clear();
        undo_project_root_ = active_root;
    }
    setEnabled(application.editable_system().has_value());
    add_component_->setEnabled(editing_enabled());
    delete_component_->setEnabled(editing_enabled());
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
    profile_label_->setVisible(profile.has_value());
    profile_resource_->setVisible(profile.has_value());
    profile_execution_->setVisible(profile.has_value());
    if (profile.has_value()) {
        profile_resource_->clear();
        for (const auto& resource : draft.resources())
            profile_resource_->addItem(QString::fromStdString(resource.name),
                                       static_cast<qulonglong>(resource.id.value()));
        profile_resource_->setCurrentIndex(
            profile_resource_->findData(static_cast<qulonglong>(profile->resource_id.value())));
        const auto execution = draft.execution_profile(profile->task_id, profile->resource_id);
        profile_execution_->setText(execution.has_value() ? QString::number(*execution)
                                                          : QString{});
    }
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
