/*** Native Qt Widgets shell for the CPSSim workbench. ***/
#pragma once

#include <QByteArray>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QString>

#include "apps/qt_gui/appearance_preferences.hpp"
#include "cpssim/application/bosch_project_factory.hpp"

#include <exception>
#include <filesystem>

class QAction;
class QCloseEvent;
class QDockWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QToolBar;
class QUndoStack;
class QVBoxLayout;

namespace cpssim::qt {

inline constexpr int qt_main_window_state_version = 1;

class QtWorkbenchBridge;
class QtSystemBuilderWidget;

struct QtFrontendPaths {
    std::filesystem::path projects_directory{"projects"};
    std::filesystem::path examples_directory{"examples"};
    std::filesystem::path bosch_reference_directory{"experiments/bosch_v10_reference"};
    std::filesystem::path bosch_fmu_library;
};

class QtMainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    explicit QtMainWindow(bool restore_user_layout = true, QWidget* parent = nullptr);

    bool home_is_active() const noexcept;
    QTabWidget* central_tabs() const noexcept { return central_tabs_; }
    QAction* run_action() const noexcept { return run_action_; }
    QAction* pause_action() const noexcept { return pause_action_; }
    QAction* reset_action() const noexcept { return reset_action_; }
    QAction* step_action() const noexcept { return step_action_; }

    QByteArray save_workbench_geometry() const;
    QByteArray save_workbench_state() const;
    bool restore_workbench_layout(const QByteArray& geometry, const QByteArray& state);
    void bind_workbench(QtWorkbenchBridge* bridge);
    void set_frontend_paths(QtFrontendPaths paths);
    bool create_generic_project_at(const std::filesystem::path& parent, std::string name);
    bool open_project_path(const std::filesystem::path& project_file);
    bool save_project_now();
    bool save_project_as_to(const std::filesystem::path& parent, std::string name);
    bool create_bosch_project_request(const BoschProjectRequest& request);

  public Q_SLOTS:
    void show_home();
    void show_workbench();
    void restore_default_layout();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    QAction* make_action(const QString& text, const QKeySequence& shortcut = {});
    QDockWidget* make_dock(const QString& title, const QString& object_name,
                           Qt::DockWidgetArea area);
    void build_actions();
    void build_central_pages();
    void build_docks();
    void build_menus_and_toolbars();
    void arrange_default_docks();
    void set_workbench_chrome_visible(bool visible);
    void load_user_layout();
    void save_user_layout() const;
    void synchronize_workbench_chrome();
    void refresh_home();
    bool confirm_unapplied_changes();
    void create_project_dialog();
    void open_project_dialog();
    void save_project_as_dialog();
    void create_bosch_project_dialog();
    void close_project_requested();
    void apply_theme();
    void report_exception(const QString& action, const std::exception& error);

    bool restore_user_layout_{true};
    QStackedWidget* pages_{nullptr};
    QWidget* home_page_{nullptr};
    QVBoxLayout* recent_projects_layout_{nullptr};
    QWidget* workbench_page_{nullptr};
    QTabWidget* central_tabs_{nullptr};
    QToolBar* dock_toolbar_{nullptr};
    QToolBar* simulation_toolbar_{nullptr};
    QList<QDockWidget*> docks_;
    QAction* new_project_action_{nullptr};
    QAction* open_project_action_{nullptr};
    QAction* save_project_action_{nullptr};
    QAction* save_project_as_action_{nullptr};
    QAction* close_project_action_{nullptr};
    QAction* run_action_{nullptr};
    QAction* pause_action_{nullptr};
    QAction* reset_action_{nullptr};
    QAction* step_action_{nullptr};
    QAction* restore_layout_action_{nullptr};
    QAction* dark_theme_action_{nullptr};
    QAction* light_theme_action_{nullptr};
    QAction* undo_action_{nullptr};
    QAction* redo_action_{nullptr};
    QtSystemBuilderWidget* system_builder_{nullptr};
    QtWorkbenchBridge* bridge_{nullptr};
    QComboBox* run_mode_combo_{nullptr};
    QComboBox* batch_unit_combo_{nullptr};
    QLineEdit* batch_size_edit_{nullptr};
    QtFrontendPaths frontend_paths_;
    QtAppearancePreferences appearance_preferences_;
    GuiTheme global_theme_{GuiTheme::Dark};
};

} // namespace cpssim::qt
