/*** Implement the native Qt Widgets workbench shell. ***/
#include "apps/qt_gui/main_window.hpp"

#include "apps/qt_gui/analysis_widgets.hpp"
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/event_table_widget.hpp"
#include "apps/qt_gui/explorer_widget.hpp"
#include "apps/qt_gui/resource_assignment_model.hpp"
#include "apps/qt_gui/runtime_widgets.hpp"
#include "apps/qt_gui/system_builder_widget.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"
#include "apps/qt_gui/workbench_style.hpp"

#include "cpssim/application/project/project_template.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

#include <limits>
#include <utility>

namespace cpssim::qt {
namespace {

constexpr auto geometry_key = "qt_workbench/geometry_v1";
constexpr auto state_key = "qt_workbench/state_v1";

QWidget* placeholder(const QString& text, QWidget* parent = nullptr) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    auto* label = new QLabel(text, widget);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);
    return widget;
}

} // namespace

QtMainWindow::QtMainWindow(bool restore_user_layout, QWidget* parent)
    : QMainWindow(parent), restore_user_layout_{restore_user_layout} {
    setObjectName("cpssimQtMainWindow");
    setWindowTitle("CPSSim[*]");
    setWindowModified(false);
    resize(1440, 900);
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks |
                   QMainWindow::AnimatedDocks);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    build_actions();
    build_central_pages();
    build_docks();
    build_menus_and_toolbars();
    arrange_default_docks();
    statusBar()->showMessage("Ready");
    show_home();
    if (restore_user_layout_) {
        load_user_layout();
    }
}

QAction* QtMainWindow::make_action(const QString& text, const QKeySequence& shortcut) {
    auto* action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    return action;
}

void QtMainWindow::build_actions() {
    new_project_action_ = make_action("Create New Project...", QKeySequence::New);
    new_project_action_->setObjectName("action.newProject");
    open_project_action_ = make_action("Open Existing Project...", QKeySequence::Open);
    open_project_action_->setObjectName("action.openProject");
    save_project_action_ = make_action("Save Project", QKeySequence::Save);
    save_project_action_->setObjectName("action.saveProject");
    save_project_as_action_ = make_action("Save Project As...", QKeySequence::SaveAs);
    save_project_as_action_->setObjectName("action.saveProjectAs");
    close_project_action_ = make_action("Close Project", QKeySequence::Close);
    close_project_action_->setObjectName("action.closeProject");

    run_action_ = make_action("Run", QKeySequence{QStringLiteral("Ctrl+R")});
    run_action_->setObjectName("action.run");
    pause_action_ = make_action("Pause", QKeySequence{QStringLiteral("Space")});
    pause_action_->setObjectName("action.pause");
    reset_action_ = make_action("Reset");
    reset_action_->setObjectName("action.reset");
    step_action_ = make_action("Step", QKeySequence{QStringLiteral("F10")});
    step_action_->setObjectName("action.step");
    restore_layout_action_ = make_action("Reset Workbench Layout");
    restore_layout_action_->setObjectName("action.resetLayout");
    undo_action_ = make_action("Undo", QKeySequence::Undo);
    undo_action_->setObjectName("action.undo");
    undo_action_->setEnabled(false);
    redo_action_ = make_action("Redo", QKeySequence::Redo);
    redo_action_->setObjectName("action.redo");
    redo_action_->setEnabled(false);
    dark_theme_action_ = make_action("Dark");
    dark_theme_action_->setObjectName("action.theme.dark");
    dark_theme_action_->setCheckable(true);
    light_theme_action_ = make_action("Light");
    light_theme_action_->setObjectName("action.theme.light");
    light_theme_action_->setCheckable(true);
    auto* themes = new QActionGroup(this);
    themes->setExclusive(true);
    themes->addAction(dark_theme_action_);
    themes->addAction(light_theme_action_);

    connect(restore_layout_action_, &QAction::triggered, this,
            &QtMainWindow::restore_default_layout);
}

void QtMainWindow::build_central_pages() {
    pages_ = new QStackedWidget(this);
    pages_->setObjectName("centralPages");
    setCentralWidget(pages_);

    home_page_ = new QWidget(pages_);
    home_page_->setObjectName("homePage");
    auto* home_layout = new QVBoxLayout(home_page_);
    home_layout->addStretch();
    auto* title = new QLabel("CPSSim", home_page_);
    title->setObjectName("homeTitle");
    title->setAlignment(Qt::AlignCenter);
    auto title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 8);
    title_font.setBold(true);
    title->setFont(title_font);
    home_layout->addWidget(title);
    auto* create = new QPushButton("Create New Project", home_page_);
    create->setObjectName("home.createProject");
    auto* open = new QPushButton("Open Existing Project", home_page_);
    open->setObjectName("home.openProject");
    auto* bosch = new QPushButton("Bosch Challenge Example", home_page_);
    bosch->setObjectName("home.boschProject");
    for (auto* button : {create, open, bosch}) {
        button->setMinimumWidth(280);
        home_layout->addWidget(button, 0, Qt::AlignHCenter);
    }
    home_layout->addStretch();
    auto* recent_title = new QLabel("Recent Projects", home_page_);
    recent_title->setAlignment(Qt::AlignHCenter);
    home_layout->addWidget(recent_title);
    recent_projects_layout_ = new QVBoxLayout;
    home_layout->addLayout(recent_projects_layout_);
    connect(create, &QPushButton::clicked, new_project_action_, &QAction::trigger);
    connect(open, &QPushButton::clicked, open_project_action_, &QAction::trigger);
    connect(bosch, &QPushButton::clicked, this, &QtMainWindow::create_bosch_project_dialog);

    workbench_page_ = new QWidget(pages_);
    workbench_page_->setObjectName("workbenchPage");
    auto* workbench_layout = new QVBoxLayout(workbench_page_);
    workbench_layout->setContentsMargins(0, 0, 0, 0);
    central_tabs_ = new QTabWidget(workbench_page_);
    central_tabs_->setObjectName("centralTabs");
    central_tabs_->setDocumentMode(true);
    central_tabs_->addTab(placeholder("Flat QtNodes Architecture", central_tabs_), "Architecture");
    central_tabs_->addTab(placeholder("Scheduling Timeline", central_tabs_), "Timeline");
    central_tabs_->addTab(placeholder("Functional Signals", central_tabs_), "Signals");
    central_tabs_->addTab(placeholder("Integrated Plot", central_tabs_), "Integrated Plot");
    workbench_layout->addWidget(central_tabs_);

    pages_->addWidget(home_page_);
    pages_->addWidget(workbench_page_);
}

QDockWidget* QtMainWindow::make_dock(const QString& title, const QString& object_name,
                                     Qt::DockWidgetArea area) {
    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(object_name);
    dock->setWidget(placeholder(title + "\nQt migration panel", dock));
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
    addDockWidget(area, dock);
    docks_.push_back(dock);
    return dock;
}

void QtMainWindow::build_docks() {
    auto* explorer = make_dock("Experiment Explorer", "dock.explorer", Qt::LeftDockWidgetArea);
    auto* builder = make_dock("System Builder", "dock.systemBuilder", Qt::LeftDockWidgetArea);
    splitDockWidget(explorer, builder, Qt::Vertical);

    auto* run = make_dock("Run Configuration", "dock.runConfiguration", Qt::RightDockWidgetArea);
    auto* inspector =
        make_dock("Runtime Inspector", "dock.runtimeInspector", Qt::RightDockWidgetArea);
    splitDockWidget(run, inspector, Qt::Vertical);

    auto* assignments =
        make_dock("Resource Assignments", "dock.resourceAssignments", Qt::BottomDockWidgetArea);
    auto* resources = make_dock("Resources", "dock.resources", Qt::BottomDockWidgetArea);
    auto* events = make_dock("Canonical Events", "dock.canonicalEvents", Qt::BottomDockWidgetArea);
    auto* results = make_dock("Results", "dock.results", Qt::BottomDockWidgetArea);
    auto* diagnostics = make_dock("Diagnostics", "dock.diagnostics", Qt::BottomDockWidgetArea);
    for (auto* dock : {resources, events, results, diagnostics}) {
        tabifyDockWidget(assignments, dock);
    }
    assignments->raise();
}

void QtMainWindow::build_menus_and_toolbars() {
    auto* file_menu = menuBar()->addMenu("&File");
    file_menu->setObjectName("menu.file");
    file_menu->addAction(new_project_action_);
    file_menu->addAction(open_project_action_);
    file_menu->addSeparator();
    file_menu->addAction(save_project_action_);
    file_menu->addAction(save_project_as_action_);
    file_menu->addAction(close_project_action_);

    auto* edit_menu = menuBar()->addMenu("&Edit");
    edit_menu->setObjectName("menu.edit");
    edit_menu->addAction(undo_action_);
    edit_menu->addAction(redo_action_);

    auto* simulation_menu = menuBar()->addMenu("&Simulation");
    simulation_menu->setObjectName("menu.simulation");
    for (auto* action : {run_action_, pause_action_, reset_action_, step_action_}) {
        simulation_menu->addAction(action);
    }
    auto* view_menu = menuBar()->addMenu("&View");
    view_menu->setObjectName("menu.view");
    for (auto* dock : docks_) {
        view_menu->addAction(dock->toggleViewAction());
    }
    view_menu->addSeparator();
    auto* theme_menu = view_menu->addMenu("Theme");
    theme_menu->addAction(dark_theme_action_);
    theme_menu->addAction(light_theme_action_);
    view_menu->addSeparator();
    view_menu->addAction(restore_layout_action_);

    simulation_toolbar_ = addToolBar("Simulation");
    simulation_toolbar_->setObjectName("toolbar.simulation");
    simulation_toolbar_->setMovable(false);
    for (auto* action : {run_action_, pause_action_, reset_action_, step_action_}) {
        simulation_toolbar_->addAction(action);
    }
    simulation_toolbar_->addSeparator();
    run_mode_combo_ = new QComboBox(simulation_toolbar_);
    run_mode_combo_->setObjectName("runMode");
    run_mode_combo_->addItems({"Live", "Fast"});
    simulation_toolbar_->addWidget(run_mode_combo_);
    batch_unit_combo_ = new QComboBox(simulation_toolbar_);
    batch_unit_combo_->setObjectName("fastBatchUnit");
    batch_unit_combo_->addItems({"Events", "Ticks"});
    simulation_toolbar_->addWidget(batch_unit_combo_);
    batch_size_edit_ = new QLineEdit(simulation_toolbar_);
    batch_size_edit_->setObjectName("fastBatchSize");
    batch_size_edit_->setMaximumWidth(90);
    simulation_toolbar_->addWidget(batch_size_edit_);
    auto* reset_batch = simulation_toolbar_->addAction("Reset Batch");
    reset_batch->setObjectName("action.resetBatch");

    dock_toolbar_ = new QToolBar("Workbench Panels", this);
    dock_toolbar_->setObjectName("toolbar.docks");
    dock_toolbar_->setMovable(false);
    dock_toolbar_->setOrientation(Qt::Vertical);
    dock_toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(Qt::LeftToolBarArea, dock_toolbar_);
    for (auto* dock : docks_) {
        dock_toolbar_->addAction(dock->toggleViewAction());
    }

    connect(dark_theme_action_, &QAction::triggered, this, [this] {
        if (bridge_ != nullptr) {
            bridge_->application().workspace().theme = GuiTheme::Dark;
            bridge_->workspace_settings_changed();
        }
        apply_theme();
    });
    connect(light_theme_action_, &QAction::triggered, this, [this] {
        if (bridge_ != nullptr) {
            bridge_->application().workspace().theme = GuiTheme::Light;
            bridge_->workspace_settings_changed();
        }
        apply_theme();
    });
    connect(run_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (bridge_ == nullptr) {
            return;
        }
        bridge_->application().workspace().run_mode =
            index == 0 ? GuiRunMode::Live : GuiRunMode::Fast;
        bridge_->workspace_settings_changed();
    });
    connect(batch_unit_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (bridge_ == nullptr) {
            return;
        }
        bridge_->application().workspace().fast_batch_unit =
            index == 0 ? GuiFastBatchUnit::Events : GuiFastBatchUnit::Ticks;
        bridge_->workspace_settings_changed();
        synchronize_workbench_chrome();
    });
    connect(batch_size_edit_, &QLineEdit::editingFinished, this, [this] {
        if (bridge_ == nullptr) {
            return;
        }
        bool valid = false;
        const auto size = batch_size_edit_->text().toULongLong(&valid);
        if (!valid || size == 0) {
            bridge_->application().set_status("Fast batch size must be positive.", true);
            Q_EMIT bridge_->statusChanged();
            synchronize_workbench_chrome();
            return;
        }
        auto& workspace = bridge_->application().workspace();
        if (workspace.fast_batch_unit == GuiFastBatchUnit::Events) {
            workspace.fast_event_batch_size = size;
        } else {
            workspace.fast_tick_batch_size = size;
        }
        bridge_->workspace_settings_changed();
    });
    connect(reset_batch, &QAction::triggered, this, [this] {
        if (bridge_ == nullptr) {
            return;
        }
        auto& workspace = bridge_->application().workspace();
        workspace.fast_batch_unit = GuiFastBatchUnit::Events;
        workspace.fast_event_batch_size = 1000;
        workspace.fast_tick_batch_size = 1000;
        bridge_->workspace_settings_changed();
        synchronize_workbench_chrome();
    });
}

void QtMainWindow::arrange_default_docks() {
    for (auto* dock : docks_) {
        dock->setFloating(false);
        dock->show();
    }
    resizeDocks(
        {findChild<QDockWidget*>("dock.explorer"), findChild<QDockWidget*>("dock.systemBuilder")},
        {420, 420}, Qt::Vertical);
    resizeDocks({findChild<QDockWidget*>("dock.runConfiguration"),
                 findChild<QDockWidget*>("dock.runtimeInspector")},
                {420, 420}, Qt::Vertical);
}

bool QtMainWindow::home_is_active() const noexcept {
    return pages_ != nullptr && pages_->currentWidget() == home_page_;
}

void QtMainWindow::set_workbench_chrome_visible(bool visible) {
    simulation_toolbar_->setVisible(visible);
    dock_toolbar_->setVisible(visible);
    for (auto* dock : docks_) {
        dock->setVisible(visible);
    }
    save_project_action_->setEnabled(visible);
    save_project_as_action_->setEnabled(visible);
    close_project_action_->setEnabled(visible);
}

void QtMainWindow::bind_workbench(QtWorkbenchBridge* bridge) {
    if (bridge_ != nullptr || bridge == nullptr) {
        return;
    }
    bridge_ = bridge;
    auto* architecture = new QtArchitectureView{*bridge_, central_tabs_};
    auto* placeholder_page = central_tabs_->widget(0);
    central_tabs_->removeTab(0);
    central_tabs_->insertTab(0, architecture, "Architecture");
    central_tabs_->setCurrentIndex(0);
    placeholder_page->deleteLater();
    const auto replace_tab = [this](int index, QWidget* replacement, const QString& title) {
        auto* old = central_tabs_->widget(index);
        central_tabs_->removeTab(index);
        central_tabs_->insertTab(index, replacement, title);
        old->deleteLater();
    };
    replace_tab(1, new QtTimelineCanvas{*bridge_, central_tabs_}, "Timeline");
    replace_tab(2, new QtSignalsWidget{*bridge_, central_tabs_}, "Signals");
    auto* integrated_plot = new QtIntegratedPlotWidget{*bridge_, central_tabs_};
    replace_tab(3, integrated_plot, "Integrated Plot");
    auto* assignments_dock = findChild<QDockWidget*>("dock.resourceAssignments");
    auto* assignments_placeholder = assignments_dock->widget();
    assignments_dock->setWidget(new QtResourceAssignmentsWidget{*bridge_, assignments_dock});
    assignments_placeholder->deleteLater();
    auto* builder_dock = findChild<QDockWidget*>("dock.systemBuilder");
    auto* builder_placeholder = builder_dock->widget();
    system_builder_ = new QtSystemBuilderWidget{*bridge_, builder_dock};
    builder_dock->setWidget(system_builder_);
    builder_placeholder->deleteLater();
    auto* explorer_dock = findChild<QDockWidget*>("dock.explorer");
    auto* explorer_placeholder = explorer_dock->widget();
    explorer_dock->setWidget(
        new QtExperimentExplorerWidget{*bridge_, *system_builder_, explorer_dock});
    explorer_placeholder->deleteLater();
    const auto replace_dock = [this](const char* name, QWidget* replacement) {
        auto* dock = findChild<QDockWidget*>(name);
        auto* old = dock->widget();
        dock->setWidget(replacement);
        old->deleteLater();
    };
    auto* run_dock = findChild<QDockWidget*>("dock.runConfiguration");
    replace_dock("dock.runConfiguration", new QtRunConfigurationWidget{*bridge_, run_dock});
    auto* inspector_dock = findChild<QDockWidget*>("dock.runtimeInspector");
    replace_dock("dock.runtimeInspector", new QtRuntimeInspectorWidget{*bridge_, inspector_dock});
    auto* resources_dock = findChild<QDockWidget*>("dock.resources");
    replace_dock("dock.resources", new QtResourcesWidget{*bridge_, resources_dock});
    auto* events_dock = findChild<QDockWidget*>("dock.canonicalEvents");
    replace_dock("dock.canonicalEvents", new QtCanonicalEventsWidget{*bridge_, events_dock});
    auto* diagnostics_dock = findChild<QDockWidget*>("dock.diagnostics");
    replace_dock("dock.diagnostics", new QtDiagnosticsWidget{*bridge_, diagnostics_dock});
    auto* results_dock = findChild<QDockWidget*>("dock.results");
    auto* results = new QtResultsWidget{*bridge_, results_dock};
    replace_dock("dock.results", results);
    connect(results, &QtResultsWidget::openIntegratedPlotRequested, this,
            [this] { central_tabs_->setCurrentIndex(3); });
    connect(undo_action_, &QAction::triggered, &system_builder_->undo_stack(), &QUndoStack::undo);
    connect(redo_action_, &QAction::triggered, &system_builder_->undo_stack(), &QUndoStack::redo);
    connect(&system_builder_->undo_stack(), &QUndoStack::canUndoChanged, undo_action_,
            &QAction::setEnabled);
    connect(&system_builder_->undo_stack(), &QUndoStack::canRedoChanged, redo_action_,
            &QAction::setEnabled);
    connect(run_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::run);
    connect(pause_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::pause);
    connect(reset_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::reset);
    connect(step_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::step);
    connect(new_project_action_, &QAction::triggered, this, &QtMainWindow::create_project_dialog);
    connect(open_project_action_, &QAction::triggered, this, &QtMainWindow::open_project_dialog);
    connect(save_project_action_, &QAction::triggered, this,
            [this] { static_cast<void>(save_project_now()); });
    connect(save_project_as_action_, &QAction::triggered, this,
            &QtMainWindow::save_project_as_dialog);
    connect(close_project_action_, &QAction::triggered, this,
            &QtMainWindow::close_project_requested);
    connect(bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::progressChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::statusChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::workspaceChanged, this, [this] {
        apply_theme();
        synchronize_workbench_chrome();
    });
    connect(bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtMainWindow::refresh_home);
    apply_theme();
    refresh_home();
    synchronize_workbench_chrome();
}

void QtMainWindow::synchronize_workbench_chrome() {
    if (bridge_ == nullptr) {
        return;
    }
    const auto& application = bridge_->application();
    if (application.screen() == GuiApplicationScreen::Home) {
        show_home();
    } else {
        show_workbench();
    }
    const auto state = application.run_state();
    run_action_->setEnabled(state == GuiRunState::Paused);
    pause_action_->setEnabled(state == GuiRunState::Running);
    reset_action_->setEnabled(state != GuiRunState::NotConfigured);
    step_action_->setEnabled(state == GuiRunState::Paused);
    setWindowModified(application.system_changes_dirty());
    const auto& workspace = application.workspace();
    run_mode_combo_->setCurrentIndex(workspace.run_mode == GuiRunMode::Live ? 0 : 1);
    batch_unit_combo_->setCurrentIndex(workspace.fast_batch_unit == GuiFastBatchUnit::Events ? 0
                                                                                             : 1);
    const auto batch_size = workspace.fast_batch_unit == GuiFastBatchUnit::Events
                                ? workspace.fast_event_batch_size
                                : workspace.fast_tick_batch_size;
    batch_size_edit_->setText(QString::number(batch_size));
    const auto fast = workspace.run_mode == GuiRunMode::Fast;
    batch_unit_combo_->setEnabled(fast);
    batch_size_edit_->setEnabled(fast);
    if (!application.status().empty()) {
        statusBar()->showMessage(QString::fromStdString(application.status()));
    } else if (application.has_active_session()) {
        const auto progress = application.progress();
        statusBar()->showMessage(QString{"Tick %1 / %2 · %3 events"}
                                     .arg(progress.current_tick)
                                     .arg(progress.stop_tick)
                                     .arg(progress.event_count));
    }
}

void QtMainWindow::set_frontend_paths(QtFrontendPaths paths) { frontend_paths_ = std::move(paths); }

void QtMainWindow::report_exception(const QString& action, const std::exception& error) {
    if (bridge_ != nullptr) {
        bridge_->application().set_status(error.what(), true);
        Q_EMIT bridge_->statusChanged();
    }
    QMessageBox::critical(this, action, QString::fromUtf8(error.what()));
}

bool QtMainWindow::confirm_unapplied_changes() {
    if (bridge_ == nullptr || !bridge_->application().system_changes_dirty()) {
        return true;
    }
    QMessageBox message{QMessageBox::Question, "Unapplied changes",
                        "The System Builder has unapplied changes.", QMessageBox::NoButton, this};
    auto* apply = message.addButton("Apply and Save", QMessageBox::AcceptRole);
    message.addButton("Discard", QMessageBox::DestructiveRole);
    auto* cancel = message.addButton(QMessageBox::Cancel);
    message.exec();
    if (message.clickedButton() == cancel) {
        return false;
    }
    const auto decision = message.clickedButton() == apply ? UnappliedSystemDecision::ApplyAndSave
                                                           : UnappliedSystemDecision::Discard;
    const auto result = bridge_->application().resolve_unapplied_changes(decision);
    if (result.status == ProjectTransitionStatus::Failed) {
        QMessageBox::critical(this, "Cannot continue", QString::fromStdString(result.diagnostic));
        return false;
    }
    return result.status == ProjectTransitionStatus::Proceed;
}

bool QtMainWindow::create_generic_project_at(const std::filesystem::path& parent,
                                             std::string name) {
    if (bridge_ == nullptr || !confirm_unapplied_changes()) {
        return false;
    }
    try {
        bridge_->create_project(make_generic_project_template(parent, std::move(name)));
        refresh_home();
        return true;
    } catch (const std::exception& error) {
        report_exception("Create Project", error);
        return false;
    }
}

bool QtMainWindow::open_project_path(const std::filesystem::path& selected) {
    if (bridge_ == nullptr || !confirm_unapplied_changes()) {
        return false;
    }
    try {
        const auto project_file =
            std::filesystem::is_directory(selected) ? selected / "project.json" : selected;
        bridge_->open_project(project_file);
        refresh_home();
        return true;
    } catch (const std::exception& error) {
        report_exception("Open Project", error);
        return false;
    }
}

bool QtMainWindow::save_project_now() {
    if (bridge_ == nullptr || !bridge_->application().has_active_project()) {
        return false;
    }
    try {
        bridge_->save_project();
        return true;
    } catch (const std::exception& error) {
        report_exception("Save Project", error);
        return false;
    }
}

bool QtMainWindow::save_project_as_to(const std::filesystem::path& parent, std::string name) {
    if (bridge_ == nullptr || !bridge_->application().has_active_project()) {
        return false;
    }
    try {
        bridge_->save_project_as(parent, std::move(name));
        refresh_home();
        return true;
    } catch (const std::exception& error) {
        report_exception("Save Project As", error);
        return false;
    }
}

bool QtMainWindow::create_bosch_project_request(const BoschProjectRequest& request) {
    if (bridge_ == nullptr || !confirm_unapplied_changes()) {
        return false;
    }
    try {
        bridge_->replace_project(create_bosch_project(request));
        refresh_home();
        return true;
    } catch (const std::exception& error) {
        report_exception("Create Bosch Project", error);
        return false;
    }
}

void QtMainWindow::create_project_dialog() {
    const auto parent = QFileDialog::getExistingDirectory(
        this, "Choose Project Parent",
        QString::fromStdString(frontend_paths_.projects_directory.string()));
    if (parent.isEmpty()) {
        return;
    }
    bool accepted = false;
    const auto name = QInputDialog::getText(this, "Create New Project",
                                            "Project name:", QLineEdit::Normal, {}, &accepted)
                          .trimmed();
    if (accepted && !name.isEmpty()) {
        static_cast<void>(create_generic_project_at(parent.toStdString(), name.toStdString()));
    }
}

void QtMainWindow::open_project_dialog() {
    const auto selected = QFileDialog::getOpenFileName(
        this, "Open CPSSim Project",
        QString::fromStdString(frontend_paths_.projects_directory.string()),
        "CPSSim Project (project.json);;JSON files (*.json)");
    if (!selected.isEmpty()) {
        static_cast<void>(open_project_path(selected.toStdString()));
    }
}

void QtMainWindow::save_project_as_dialog() {
    const auto parent = QFileDialog::getExistingDirectory(
        this, "Save Project As",
        QString::fromStdString(frontend_paths_.projects_directory.string()));
    if (parent.isEmpty()) {
        return;
    }
    bool accepted = false;
    const auto name = QInputDialog::getText(this, "Save Project As",
                                            "New project name:", QLineEdit::Normal, {}, &accepted)
                          .trimmed();
    if (accepted && !name.isEmpty()) {
        static_cast<void>(save_project_as_to(parent.toStdString(), name.toStdString()));
    }
}

void QtMainWindow::create_bosch_project_dialog() {
    QWizard wizard(this);
    wizard.setWindowTitle("Bosch Challenge Project");

    auto* trajectory_page = new QWizardPage;
    trajectory_page->setTitle("1. Trajectory");
    auto* trajectory_layout = new QFormLayout(trajectory_page);
    auto* trajectory = new QComboBox;
    trajectory->addItems({"example_v_10", "example_v_12_5", "example_v_15", "Custom directory"});
    auto* custom_trajectory = new QLineEdit;
    auto* browse_trajectory = new QPushButton("Browse...");
    auto* custom_row = new QWidget;
    auto* custom_layout = new QHBoxLayout(custom_row);
    custom_layout->setContentsMargins(0, 0, 0, 0);
    custom_layout->addWidget(custom_trajectory);
    custom_layout->addWidget(browse_trajectory);
    trajectory_layout->addRow("Trajectory", trajectory);
    trajectory_layout->addRow("Custom directory", custom_row);
    wizard.addPage(trajectory_page);

    auto* scenario_page = new QWizardPage;
    scenario_page->setTitle("2. Scenario");
    auto* scenario_layout = new QFormLayout(scenario_page);
    auto* scenario = new QComboBox;
    scenario->addItems({"Dedicated", "Shared cloud"});
    scenario_layout->addRow("Scenario", scenario);
    wizard.addPage(scenario_page);

    auto* horizon_page = new QWizardPage;
    horizon_page->setTitle("3. Horizon");
    auto* horizon_layout = new QFormLayout(horizon_page);
    auto* horizon = new QComboBox;
    horizon->addItems({"Complete trajectory", "Custom stop tick"});
    auto* stop_tick = new QLineEdit("150000");
    horizon_layout->addRow("Horizon", horizon);
    horizon_layout->addRow("Stop tick", stop_tick);
    wizard.addPage(horizon_page);

    auto* project_page = new QWizardPage;
    project_page->setTitle("4. Project");
    auto* project_layout = new QFormLayout(project_page);
    auto* project_name = new QLineEdit("bosch-project");
    auto* parent =
        new QLineEdit(QString::fromStdString(frontend_paths_.projects_directory.string()));
    auto* browse_parent = new QPushButton("Browse...");
    auto* parent_row = new QWidget;
    auto* parent_layout = new QHBoxLayout(parent_row);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    parent_layout->addWidget(parent);
    parent_layout->addWidget(browse_parent);
    project_layout->addRow("Project name", project_name);
    project_layout->addRow("Parent directory", parent_row);
    wizard.addPage(project_page);

    auto* review_page = new QWizardPage;
    review_page->setTitle("5. Review and Create");
    auto* review_layout = new QVBoxLayout(review_page);
    auto* review = new QLabel("Review the selected trajectory, scenario, horizon, and project "
                              "location, then choose Finish.");
    review->setWordWrap(true);
    review_layout->addWidget(review);
    wizard.addPage(review_page);

    const auto update_custom = [trajectory, custom_row] {
        custom_row->setEnabled(trajectory->currentIndex() == 3);
    };
    connect(trajectory, &QComboBox::currentIndexChanged, this,
            [update_custom](int) { update_custom(); });
    update_custom();
    connect(horizon, &QComboBox::currentIndexChanged, stop_tick,
            [horizon, stop_tick](int) { stop_tick->setEnabled(horizon->currentIndex() == 1); });
    stop_tick->setEnabled(false);
    connect(browse_trajectory, &QPushButton::clicked, &wizard, [&] {
        const auto directory = QFileDialog::getExistingDirectory(
            &wizard, "Choose Trajectory Directory", custom_trajectory->text());
        if (!directory.isEmpty()) {
            custom_trajectory->setText(directory);
        }
    });
    connect(browse_parent, &QPushButton::clicked, &wizard, [&] {
        const auto directory =
            QFileDialog::getExistingDirectory(&wizard, "Choose Project Parent", parent->text());
        if (!directory.isEmpty()) {
            parent->setText(directory);
        }
    });
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }
    std::optional<Tick> requested_stop;
    if (horizon->currentIndex() == 1) {
        bool valid = false;
        const auto value = stop_tick->text().toLongLong(&valid);
        if (!valid || value < 0) {
            QMessageBox::critical(this, "Create Bosch Project", "Stop tick must be nonnegative.");
            return;
        }
        requested_stop = static_cast<Tick>(value);
    }
    const auto trajectory_path =
        trajectory->currentIndex() == 3
            ? std::filesystem::path{custom_trajectory->text().toStdString()}
            : frontend_paths_.examples_directory / trajectory->currentText().toStdString();
    BoschProjectRequest request{.parent_directory = parent->text().toStdString(),
                                .name = project_name->text().trimmed().toStdString(),
                                .trajectory_directory = trajectory_path,
                                .scenario = scenario->currentIndex() == 0
                                                ? BoschReferenceScenario::Dedicated
                                                : BoschReferenceScenario::SharedCloud,
                                .stop_tick = requested_stop,
                                .reference_root = frontend_paths_.bosch_reference_directory,
                                .shared_library = frontend_paths_.bosch_fmu_library};
    static_cast<void>(create_bosch_project_request(request));
}

void QtMainWindow::close_project_requested() {
    if (bridge_ != nullptr && confirm_unapplied_changes()) {
        bridge_->close_project();
        refresh_home();
    }
}

void QtMainWindow::refresh_home() {
    if (bridge_ == nullptr || recent_projects_layout_ == nullptr) {
        return;
    }
    while (auto* item = recent_projects_layout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto& recent = bridge_->application().recent_projects();
    recent.refresh_availability();
    if (recent.entries().empty()) {
        recent_projects_layout_->addWidget(new QLabel("No recent projects", home_page_), 0,
                                           Qt::AlignHCenter);
        return;
    }
    const auto entries = recent.entries();
    for (const auto& entry : entries) {
        auto* row = new QWidget(home_page_);
        auto* layout = new QHBoxLayout(row);
        const auto label = QString::fromStdString(entry.project_file.string()) +
                           (entry.available ? QString{} : QString{" (unavailable)"});
        auto* open = new QPushButton(label, row);
        open->setEnabled(entry.available);
        open->setToolTip(QString::fromStdString(entry.project_file.string()));
        auto* remove = new QPushButton("Remove", row);
        layout->addWidget(open, 1);
        layout->addWidget(remove);
        const auto path = entry.project_file;
        connect(open, &QPushButton::clicked, this,
                [this, path] { static_cast<void>(open_project_path(path)); });
        connect(remove, &QPushButton::clicked, this, [this, path] {
            bridge_->application().recent_projects().remove(path);
            bridge_->application().persist_recent_history();
            refresh_home();
        });
        recent_projects_layout_->addWidget(row);
    }
}

void QtMainWindow::apply_theme() {
    const auto theme =
        bridge_ == nullptr ? GuiTheme::Dark : bridge_->application().workspace().theme;
    dark_theme_action_->setChecked(theme == GuiTheme::Dark);
    light_theme_action_->setChecked(theme == GuiTheme::Light);
    QApplication::setStyle("Fusion");
    QApplication::setPalette(workbench_palette(theme));
}

void QtMainWindow::show_home() {
    pages_->setCurrentWidget(home_page_);
    set_workbench_chrome_visible(false);
    statusBar()->showMessage("No active project");
}

void QtMainWindow::show_workbench() {
    pages_->setCurrentWidget(workbench_page_);
    set_workbench_chrome_visible(true);
    statusBar()->showMessage("Workbench ready");
}

void QtMainWindow::restore_default_layout() {
    arrange_default_docks();
    if (home_is_active()) {
        set_workbench_chrome_visible(false);
    }
}

QByteArray QtMainWindow::save_workbench_geometry() const { return saveGeometry(); }

QByteArray QtMainWindow::save_workbench_state() const {
    return saveState(qt_main_window_state_version);
}

bool QtMainWindow::restore_workbench_layout(const QByteArray& geometry, const QByteArray& state) {
    const auto geometry_ok = geometry.isEmpty() || restoreGeometry(geometry);
    const auto state_ok = state.isEmpty() || restoreState(state, qt_main_window_state_version);
    return geometry_ok && state_ok;
}

void QtMainWindow::load_user_layout() {
    QSettings settings("CPSSim", "CPSSim Qt GUI");
    static_cast<void>(restore_workbench_layout(settings.value(geometry_key).toByteArray(),
                                               settings.value(state_key).toByteArray()));
}

void QtMainWindow::save_user_layout() const {
    QSettings settings("CPSSim", "CPSSim Qt GUI");
    settings.setValue(geometry_key, save_workbench_geometry());
    settings.setValue(state_key, save_workbench_state());
}

void QtMainWindow::closeEvent(QCloseEvent* event) {
    if (!confirm_unapplied_changes()) {
        event->ignore();
        return;
    }
    if (restore_user_layout_) {
        save_user_layout();
    }
    if (bridge_ != nullptr) {
        bridge_->shutdown();
    }
    QMainWindow::closeEvent(event);
}

} // namespace cpssim::qt
