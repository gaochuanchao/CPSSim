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
class QPushButton;
class QStackedWidget;
class QTableView;

namespace cpssim::qt {

class QtStructuralEditController;

struct QtTaskExecutionProfileRow {
    ResourceId resource_id;
    QString resource_name;
    bool accessible{true};
    std::optional<Tick> execution_time;
};

class QtTaskExecutionProfileEditModel final : public QAbstractTableModel {
  public:
    enum Column { Resource, Accessible, ExecutionTime, Status, Count };

    explicit QtTaskExecutionProfileEditModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    void load(TaskId task_id, const EditableSystemDraft& draft);
    const std::vector<QtTaskExecutionProfileRow>& rows() const { return rows_; }
    bool complete() const;
    const QtTaskExecutionProfileRow* row_at(int row) const;

  private:
    std::vector<QtTaskExecutionProfileRow> rows_;
};

class QtSystemBuilderWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit QtSystemBuilderWidget(QtWorkbenchBridge& bridge,
                                   QtStructuralEditController& edits,
                                   QWidget* parent = nullptr);

    void refresh();
    bool commit_pending_edits();
    bool create_component(StructuralSection section);
    bool duplicate_selected();
    bool delete_selected(bool confirmed);

  Q_SIGNALS:
    void taskCreated(TaskId task_id);
    void completeSaveRequested();

  private:
    void build_pages();
    void connect_editors();
    bool commit_route_latency();
    void refresh_system_page();
    void refresh_resource_page(ResourceId resource_id);
    void refresh_task_page(TaskId task_id,
                           std::optional<DraftExecutionProfileKey> profile = std::nullopt);
    void refresh_connection_page();
    void refresh_diagnostics();
    bool editing_enabled() const;
    ProjectSystemEditPolicy edit_policy() const;
    void open_execution_profile_dialog();
    bool task_profiles_complete(TaskId task_id) const;
    void refresh_profile_button_state(TaskId task_id);

    QtWorkbenchBridge& bridge_;
    QtStructuralEditController& edits_;
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
    QPushButton* profile_button_{nullptr};
    QLabel* system_heading_{nullptr};
    QLabel* resource_heading_{nullptr};
    QLabel* task_heading_{nullptr};
    QLabel* connection_heading_{nullptr};
    QComboBox* connection_source_{nullptr};
    QComboBox* connection_destination_{nullptr};
    QComboBox* connection_kind_{nullptr};
    QLabel* route_delay_label_{nullptr};
    QLineEdit* route_delay_{nullptr};
    bool refreshing_{false};
};

} // namespace cpssim::qt
