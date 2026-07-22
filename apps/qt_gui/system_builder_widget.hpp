/*** Docked selected-item System Builder and component library. ***/
#pragma once

#include "apps/qt_gui/workbench_bridge.hpp"

#include <QAbstractTableModel>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableView;
class QUndoStack;

namespace cpssim::qt {

struct QtTaskExecutionProfileRow {
    ResourceId resource_id;
    QString resource_name;
    std::optional<Tick> execution_time;
};

class QtTaskExecutionProfileModel final : public QAbstractTableModel {
  public:
    enum Column { Resource, ExecutionTime, Accessible, Status, Count };
    using EditCallback = std::function<bool(ResourceId, std::optional<Tick>)>;

    explicit QtTaskExecutionProfileModel(EditCallback edit_callback, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    void set_task(std::optional<TaskId> task_id, const EditableSystemDraft* draft,
                  bool editing_enabled);
    const QtTaskExecutionProfileRow* row_at(int row) const;

  private:
    EditCallback edit_callback_;
    std::vector<QtTaskExecutionProfileRow> rows_;
    bool editing_enabled_{false};
};

class QtSystemBuilderWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtSystemBuilderWidget(QtWorkbenchBridge& bridge, QWidget* parent = nullptr);

    QUndoStack& undo_stack() noexcept { return *undo_stack_; }
    void refresh();
    bool create_component(StructuralSection section);
    bool duplicate_selected();
    bool delete_selected(bool confirmed);

  Q_SIGNALS:
    void taskCreated(TaskId task_id);

  private:
    using DraftMutator = std::function<void(EditableSystemDraft&, std::vector<DraftTaskAssignment>&,
                                            StructuralSelection&)>;

    void build_pages();
    void connect_editors();
    bool push_mutation(const QString& text, DraftMutator mutator);
    void refresh_system_page();
    void refresh_resource_page(ResourceId resource_id);
    void refresh_task_page(TaskId task_id,
                           std::optional<DraftExecutionProfileKey> profile = std::nullopt);
    void refresh_connection_page();
    void refresh_diagnostics();
    bool editing_enabled() const;
    ProjectSystemEditPolicy edit_policy() const;

    QtWorkbenchBridge& bridge_;
    QUndoStack* undo_stack_{nullptr};
    QStackedWidget* pages_{nullptr};
    QWidget* empty_page_{nullptr};
    QWidget* system_page_{nullptr};
    QWidget* resource_page_{nullptr};
    QWidget* task_page_{nullptr};
    QWidget* connection_page_{nullptr};
    QLabel* empty_message_{nullptr};
    QLabel* diagnostic_summary_{nullptr};
    QLabel* protected_help_{nullptr};
    QLabel* system_diagnostic_{nullptr};
    QLabel* resource_diagnostic_{nullptr};
    QLabel* task_diagnostic_{nullptr};
    QLabel* connection_diagnostic_{nullptr};
    QLineEdit* tick_period_{nullptr};
    QComboBox* preemption_{nullptr};
    QLineEdit* resource_id_{nullptr};
    QLineEdit* resource_name_{nullptr};
    QLineEdit* task_id_{nullptr};
    QLineEdit* task_name_{nullptr};
    QLineEdit* task_period_{nullptr};
    QLineEdit* task_deadline_{nullptr};
    QLineEdit* task_offset_{nullptr};
    QLineEdit* task_priority_{nullptr};
    QComboBox* task_assignment_{nullptr};
    QLabel* assignment_status_{nullptr};
    QtTaskExecutionProfileModel* profile_model_{nullptr};
    QTableView* profile_table_{nullptr};
    QComboBox* connection_source_{nullptr};
    QComboBox* connection_destination_{nullptr};
    QLabel* connection_kind_{nullptr};
    QLabel* connection_latency_{nullptr};
    QLineEdit* route_send_offset_{nullptr};
    QLineEdit* route_delay_{nullptr};
    std::optional<std::filesystem::path> undo_project_root_;
    bool refreshing_{false};
};

} // namespace cpssim::qt
