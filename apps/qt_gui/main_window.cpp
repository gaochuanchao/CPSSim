/*** Implement the native Qt Widgets workbench shell. ***/
#include "apps/qt_gui/main_window.hpp"

#include "apps/qt_gui/architecture_view.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

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
    connect(create, &QPushButton::clicked, new_project_action_, &QAction::trigger);
    connect(open, &QPushButton::clicked, open_project_action_, &QAction::trigger);

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
    view_menu->addAction(restore_layout_action_);

    simulation_toolbar_ = addToolBar("Simulation");
    simulation_toolbar_->setObjectName("toolbar.simulation");
    simulation_toolbar_->setMovable(false);
    for (auto* action : {run_action_, pause_action_, reset_action_, step_action_}) {
        simulation_toolbar_->addAction(action);
    }

    dock_toolbar_ = new QToolBar("Workbench Panels", this);
    dock_toolbar_->setObjectName("toolbar.docks");
    dock_toolbar_->setMovable(false);
    dock_toolbar_->setOrientation(Qt::Vertical);
    dock_toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(Qt::LeftToolBarArea, dock_toolbar_);
    for (auto* dock : docks_) {
        dock_toolbar_->addAction(dock->toggleViewAction());
    }
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
    connect(run_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::run);
    connect(pause_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::pause);
    connect(reset_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::reset);
    connect(step_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::step);
    connect(close_project_action_, &QAction::triggered, bridge_, &QtWorkbenchBridge::close_project);
    connect(bridge_, &QtWorkbenchBridge::applicationStateChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::progressChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
    connect(bridge_, &QtWorkbenchBridge::statusChanged, this,
            &QtMainWindow::synchronize_workbench_chrome);
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
    if (restore_user_layout_) {
        save_user_layout();
    }
    QMainWindow::closeEvent(event);
}

} // namespace cpssim::qt
