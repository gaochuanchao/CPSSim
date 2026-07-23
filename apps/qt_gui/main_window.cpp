/*** Implement the native Qt Widgets workbench shell. ***/
#include "apps/qt_gui/main_window.hpp"

#include "apps/qt_gui/analysis_widgets.hpp"
#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/event_table_widget.hpp"
#include "apps/qt_gui/explorer_widget.hpp"
#include "apps/qt_gui/resource_assignment_model.hpp"
#include "apps/qt_gui/runtime_widgets.hpp"
#include "apps/qt_gui/structural_edit_controller.hpp"
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
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

#include <algorithm>
#include <limits>
#include <utility>

namespace cpssim::qt {
namespace {

constexpr auto geometry_key = "qt_workbench/geometry_v2";
constexpr auto state_key = "qt_workbench/state_v2";

QWidget* placeholder(const QString& text, QWidget* parent = nullptr) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    auto* label = new QLabel(text, widget);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);
    return widget;
}

void install_dock_content(
    QDockWidget* dock,
    QWidget* content) {

    if (dock == nullptr || content == nullptr) {
        return;
    }

    /*
     * Mark the actual panel widget rather than the QDockWidget
     * wrapper. The application stylesheet uses this property
     * to draw the visible one-pixel panel frame.
     */
    content->setProperty(
        "cpssimDockContent",
        true);

    content->setAttribute(
        Qt::WA_StyledBackground,
        true);

    dock->setWidget(content);
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
    workbench_dock_state_ = save_workbench_state();
    statusBar()->showMessage("Ready");
    auto* bottom_toggle = new QToolButton(statusBar());
    bottom_toggle->setObjectName("bottomAnalysis.toggle");
    bottom_toggle->setText("⌄");
    bottom_toggle->setToolTip("Collapse or restore bottom analysis area (Ctrl+J)");
    statusBar()->addPermanentWidget(bottom_toggle);
    connect(bottom_toggle, &QToolButton::clicked, collapse_bottom_action_, &QAction::trigger);
    if (restore_user_layout_) {
        load_user_layout();
    }
    global_theme_ = appearance_preferences_.theme();
    apply_theme();
    show_home();
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
    collapse_bottom_action_ = make_action("Collapse Bottom Analysis Area", QKeySequence{"Ctrl+J"});
    collapse_bottom_action_->setObjectName("action.collapseBottom");
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
    connect(collapse_bottom_action_, &QAction::triggered, this,
            &QtMainWindow::toggle_bottom_analysis);
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
    auto* recent_container = new QWidget(home_page_);
    recent_container->setObjectName("home.recentProjects");
    recent_container->setMinimumWidth(420);
    recent_container->setMaximumWidth(720);
    recent_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    recent_projects_layout_ = new QVBoxLayout(recent_container);
    recent_projects_layout_->setSpacing(6);
    home_layout->addWidget(recent_container, 0, Qt::AlignHCenter);
    connect(create, &QPushButton::clicked, new_project_action_, &QAction::trigger);
    connect(open, &QPushButton::clicked, open_project_action_, &QAction::trigger);
    connect(bosch, &QPushButton::clicked, this, &QtMainWindow::create_bosch_project_dialog);

    workbench_page_ = new QWidget(pages_);
    workbench_page_->setObjectName("workbenchPage");
    workbench_page_->setAttribute(Qt::WA_StyledBackground, true);
    auto* workbench_layout = new QVBoxLayout(workbench_page_);
    workbench_layout->setContentsMargins(1, 1, 1, 1);
    workbench_layout->setSpacing(0);
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

QDockWidget* QtMainWindow::make_dock(
    const QString& title,
    const QString& object_name,
    Qt::DockWidgetArea area) {

    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(object_name);

    install_dock_content(
        dock,
        placeholder(
            title + "\nQt migration panel",
            dock));

    dock->setFeatures(
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable);

    dock->setMinimumSize(120, 80);
    dock->widget()->setMinimumSize(0, 0);
    dock->widget()->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

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
    bottom_docks_ = {assignments, resources, events, results, diagnostics};
    for (auto* dock : bottom_docks_) {
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        auto* redock = new QAction("Dock in Bottom Analysis Group", dock);
        redock->setObjectName("action.dockBottom." + dock->objectName());
        dock->addAction(redock);
        dock->setContextMenuPolicy(Qt::ActionsContextMenu);
        connect(redock, &QAction::triggered, this, [this, dock] { dock_in_bottom_analysis(dock); });
        connect(dock, &QDockWidget::visibilityChanged, this, [this, dock](bool visible) {
            if (visible && !bottom_collapsed_ && dockWidgetArea(dock) == Qt::BottomDockWidgetArea) {
                bottom_selected_ = dock;
            }
        });
    }
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
    view_menu->addAction(collapse_bottom_action_);
    auto* dock_bottom_menu = view_menu->addMenu("Dock in Bottom Analysis Group");
    for (auto* dock : bottom_docks_) {
        dock_bottom_menu->addAction(dock->actions().front());
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

    connect(dark_theme_action_, &QAction::triggered, this, [this] {
        global_theme_ = GuiTheme::Dark;
        appearance_preferences_.set_theme(global_theme_);
        apply_theme();
    });
    connect(light_theme_action_, &QAction::triggered, this, [this] {
        global_theme_ = GuiTheme::Light;
        appearance_preferences_.set_theme(global_theme_);
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
    resizeDocks({findChild<QDockWidget*>("dock.explorer")}, {320}, Qt::Horizontal);
    resizeDocks({findChild<QDockWidget*>("dock.runConfiguration")}, {300}, Qt::Horizontal);
    resizeDocks({findChild<QDockWidget*>("dock.resourceAssignments")}, {220}, Qt::Vertical);
}

bool QtMainWindow::home_is_active() const noexcept {
    return pages_ != nullptr && pages_->currentWidget() == home_page_;
}

void QtMainWindow::set_workbench_chrome_visible(bool visible) {
    simulation_toolbar_->setVisible(visible);
    if (!visible) {
        for (auto* dock : docks_) {
            dock->hide();
        }
    }
    save_project_action_->setEnabled(visible);
    save_project_as_action_->setEnabled(visible);
    close_project_action_->setEnabled(visible);
    restore_layout_action_->setEnabled(visible);
    undo_action_->setEnabled(visible && structural_edits_ != nullptr &&
                             structural_edits_->undo_stack().canUndo());
    redo_action_->setEnabled(visible && structural_edits_ != nullptr &&
                             structural_edits_->undo_stack().canRedo());
    for (auto* dock : docks_) {
        dock->toggleViewAction()->setEnabled(visible);
    }
    collapse_bottom_action_->setEnabled(visible);
}

void QtMainWindow::bind_workbench(QtWorkbenchBridge* bridge) {
    if (bridge_ != nullptr || bridge == nullptr) {
        return;
    }
    bridge_ = bridge;
    structural_edits_ = new QtStructuralEditController{*bridge_, this};
    auto* architecture = new QtArchitectureView{*bridge_, *structural_edits_, central_tabs_};
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
    install_dock_content(assignments_dock, new QtResourceAssignmentsWidget{*bridge_, assignments_dock});
    assignments_placeholder->deleteLater();
    auto* builder_dock = findChild<QDockWidget*>("dock.systemBuilder");
    auto* builder_placeholder = builder_dock->widget();
    system_builder_ = new QtSystemBuilderWidget{*bridge_, *structural_edits_, builder_dock};
    connect(system_builder_, &QtSystemBuilderWidget::taskCreated, architecture,
            &QtArchitectureView::place_task_near_view_center);
    connect(system_builder_, &QtSystemBuilderWidget::completeSaveRequested, this,
            [this] { static_cast<void>(save_project_now()); });
    connect(architecture, &QtArchitectureView::editSelectionRequested, this,
            [this] {
                auto* dock = findChild<QDockWidget*>("dock.systemBuilder");
                if (dock == nullptr || system_builder_ == nullptr) {
                    return;
                }
                dock->show();
                dock->raise();
                system_builder_->setFocus();
            });
    install_dock_content(builder_dock, system_builder_);
    builder_placeholder->deleteLater();
    auto* explorer_dock = findChild<QDockWidget*>("dock.explorer");
    auto* explorer_placeholder = explorer_dock->widget();
    install_dock_content( explorer_dock, new QtExperimentExplorerWidget{*bridge_, *system_builder_, explorer_dock});
    explorer_placeholder->deleteLater();
    const auto replace_dock = [this](const char* name, QWidget* replacement) {
        auto* dock =
            findChild<QDockWidget*>(name);

        auto* old = dock->widget();

        install_dock_content(
            dock,
            replacement);

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
    connect(undo_action_, &QAction::triggered, &structural_edits_->undo_stack(),
            &QUndoStack::undo);
    connect(redo_action_, &QAction::triggered, &structural_edits_->undo_stack(),
            &QUndoStack::redo);
    connect(&structural_edits_->undo_stack(), &QUndoStack::canUndoChanged, undo_action_,
            [this](bool enabled) { undo_action_->setEnabled(enabled && !home_is_active()); });
    connect(&structural_edits_->undo_stack(), &QUndoStack::canRedoChanged, redo_action_,
            [this](bool enabled) { redo_action_->setEnabled(enabled && !home_is_active()); });
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
    connect(bridge_, &QtWorkbenchBridge::workspaceChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::completedResultChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
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
    if (application.screen() == GuiApplicationScreen::Home && !home_is_active()) {
        show_home();
    } else if (application.screen() == GuiApplicationScreen::Workbench && home_is_active()) {
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
    if (bridge_ == nullptr) {
        return true;
    }

    // Commit any pending System Builder editor before evaluating dirty state.
    // This ensures focus-loss-uncommitted edits are reflected in the dirty
    // check and the confirmation dialog provides an accurate choice.
    if (system_builder_ != nullptr) {
        if (!system_builder_->commit_pending_edits()) {
            QMessageBox::critical(this, "Cannot apply",
                                  "System Builder contains invalid input.");
            return false;
        }
    }

    if (!bridge_->application().system_changes_dirty()) {
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
    if (message.clickedButton() == apply) {
        // Use the same complete-save workflow as Ctrl+S.
        // commit_pending_edits was already called above, so no need to repeat.
        if (!bridge_->apply_and_save_project()) {
            return false;
        }
        setWindowModified(false);
        return true;
    }
    // Discard: leave window dirty state as-is (close will clear it).
    return true;
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
        // Commit any pending System Builder editor.
        if (system_builder_ != nullptr) {
            if (!system_builder_->commit_pending_edits()) {
                return false;
            }
        }

        // Apply and save the complete project.
        if (!bridge_->apply_and_save_project()) {
            return false;
        }

        // Update title bar and dirty state after successful save.
        setWindowModified(false);
        statusBar()->showMessage("Project saved.", 3000);
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
        // Commit pending System Builder edits first.
        if (system_builder_ != nullptr) {
            if (!system_builder_->commit_pending_edits()) {
                return false;
            }
        }

        // Apply draft changes before saving-as to ensure the copy includes
        // all System Builder edits.
        if (bridge_->application().editable_system().has_value() &&
            bridge_->application().system_changes_dirty()) {
            if (!bridge_->apply_and_save_project()) {
                return false;
            }
        }

        bridge_->save_project_as(parent, std::move(name));
        setWindowModified(false);
        statusBar()->showMessage("Project saved.", 3000);
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
        layout->setContentsMargins(4, 2, 4, 2);
        auto* description = new QWidget(row);
        auto* description_layout = new QVBoxLayout(description);
        description_layout->setContentsMargins(0, 0, 0, 0);
        description_layout->setSpacing(1);
        const auto project_name =
            QString::fromStdString(entry.project_file.parent_path().filename().string());
        auto* name = new QLabel(project_name, description);
        auto* path_label = new QLabel(description);
        const auto full_path = QString::fromStdString(entry.project_file.string());
        path_label->setText(path_label->fontMetrics().elidedText(full_path, Qt::ElideMiddle, 480));
        path_label->setToolTip(full_path);
        description_layout->addWidget(name);
        description_layout->addWidget(path_label);
        auto* open = new QPushButton("Open", row);
        open->setObjectName("recent.open");
        open->setMaximumWidth(84);
        open->setEnabled(entry.available);
        open->setToolTip(full_path);
        auto* remove = new QPushButton("Remove", row);
        remove->setObjectName("recent.remove");
        remove->setMaximumWidth(84);
        layout->addWidget(description, 1);
        layout->addWidget(open);
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
    dark_theme_action_->setChecked(
        global_theme_ == GuiTheme::Dark);

    light_theme_action_->setChecked(
        global_theme_ == GuiTheme::Light);

    apply_workbench_theme(global_theme_);

    const QString boundary_color =
        global_theme_ == GuiTheme::Dark
            ? QStringLiteral("#555b64")
            : QStringLiteral("#aeb4bd");

    const QString gutter_color =
        global_theme_ == GuiTheme::Dark
            ? QStringLiteral("#26292e")
            : QStringLiteral("#f5f6f8");

    qApp->setStyleSheet(
        QStringLiteral(
            R"(
            /*
             * Make the unused part of the dock resizing gutter
             * blend into the normal workbench background.
             */
            QMainWindow#cpssimQtMainWindow::separator {
                background: %2;
            }

            QMainWindow#cpssimQtMainWindow::separator:hover {
                background: %2;
            }

            /*
             * Give every docked workbench panel a consistent
             * one-pixel outer boundary.
             */
            QWidget[cpssimDockContent="true"] {
                border: 1px solid %1;
            }

            /*
             * The middle workbench is not a QDockWidget.
             * Apply the same boundary to its outer container.
             */
            QWidget#workbenchPage {
                border: 1px solid %1;
            }
            )")
            .arg(boundary_color, gutter_color));

    if (bridge_ != nullptr) {
        Q_EMIT bridge_->appearanceChanged();
    }
}

void QtMainWindow::toggle_bottom_analysis() {
    if (home_is_active()) {
        return;
    }
    if (!bottom_collapsed_) {
        bottom_visible_before_collapse_.clear();
        for (auto* dock : bottom_docks_) {
            if (dock->isVisible()) {
                bottom_visible_before_collapse_.insert(dock->objectName());
                bottom_height_ = std::max(bottom_height_, dock->height());
            }
            dock->hide();
        }
        bottom_collapsed_ = true;
    } else {
        auto* anchor = bottom_docks_.front();
        for (auto* dock : bottom_docks_) {
            if (bottom_visible_before_collapse_.contains(dock->objectName())) {
                addDockWidget(Qt::BottomDockWidgetArea, dock);
                if (dock != anchor) {
                    tabifyDockWidget(anchor, dock);
                }
                dock->show();
            }
        }
        if (bottom_selected_ != nullptr && bottom_selected_->isVisible()) {
            bottom_selected_->raise();
        }
        resizeDocks({anchor}, {bottom_height_}, Qt::Vertical);
        bottom_collapsed_ = false;
    }
    update_bottom_action();
}

void QtMainWindow::dock_in_bottom_analysis(QDockWidget* dock) {
    if (dock == nullptr || !bottom_docks_.contains(dock)) {
        return;
    }
    auto* anchor = bottom_docks_.front();
    dock->setFloating(false);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    if (dock != anchor) {
        tabifyDockWidget(anchor, dock);
    }
    dock->show();
    dock->raise();
    bottom_selected_ = dock;
}

void QtMainWindow::update_bottom_action() {
    collapse_bottom_action_->setText(bottom_collapsed_ ? "Restore Bottom Analysis Area"
                                                       : "Collapse Bottom Analysis Area");
}

void QtMainWindow::show_home() {
    if (!home_is_active()) {
        workbench_dock_state_ = save_workbench_state();
    }
    pages_->setCurrentWidget(home_page_);
    set_workbench_chrome_visible(false);
    statusBar()->showMessage("No active project");
}

void QtMainWindow::show_workbench() {
    if (!home_is_active()) {
        return;
    }
    pages_->setCurrentWidget(workbench_page_);
    set_workbench_chrome_visible(true);
    if (!workbench_dock_state_.isEmpty()) {
        static_cast<void>(restoreState(workbench_dock_state_, qt_main_window_state_version));
    } else {
        arrange_default_docks();
    }
    statusBar()->showMessage("Workbench ready");
}

void QtMainWindow::restore_default_layout() {
    arrange_default_docks();
    workbench_dock_state_ = save_workbench_state();
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
    const auto state = settings.value(state_key).toByteArray();
    if (restore_workbench_layout(settings.value(geometry_key).toByteArray(), state)) {
        workbench_dock_state_ = save_workbench_state();
    }
}

void QtMainWindow::save_user_layout() const {
    QSettings settings("CPSSim", "CPSSim Qt GUI");
    settings.setValue(geometry_key, save_workbench_geometry());
    settings.setValue(state_key, home_is_active() ? workbench_dock_state_ : save_workbench_state());
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
