/*** Start the native Qt Widgets CPSSim frontend. ***/
#include "apps/qt_gui/main_window.hpp"
#include "apps/qt_gui/workbench_bridge.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/bosch_workbench_services.hpp"
#include "cpssim/application/recent_projects.hpp"
#include "cpssim/application/workbench_application.hpp"

#include <QApplication>
#include <QCoreApplication>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

std::unique_ptr<cpssim::WorkbenchApplication>
make_workbench(const std::filesystem::path& executable_path,
               const std::filesystem::path& repository_root) {
    return std::make_unique<cpssim::WorkbenchApplication>(
        cpssim::WorkbenchApplicationPaths{.projects_directory = repository_root / "projects",
                                          .preferences_file =
                                              cpssim::default_gui_preferences_file()},
        cpssim::make_bosch_workbench_services(repository_root / "experiments/bosch_v10_reference",
                                              cpssim::resolve_bundled_bosch_fmu(executable_path)));
}

} // namespace

int main(int argc, char** argv) {
    try {
        for (int index = 1; index < argc; ++index) {
            if (std::string_view{argv[index]} == "--help") {
                std::cout << "Usage: cpssim_gui [project.json]\n";
                return 0;
            }
        }
        if (argc > 2) {
            throw std::invalid_argument{"too many arguments; use --help for usage"};
        }
        QApplication application(argc, argv);
        QCoreApplication::setOrganizationName("CPSSim");
        QCoreApplication::setApplicationName("CPSSim Qt GUI");
        cpssim::qt::QtMainWindow window;
        const auto executable = std::filesystem::absolute(argv[0]);
        const auto repository_root = std::filesystem::current_path();
        window.set_frontend_paths(
            {.projects_directory = repository_root / "projects",
             .examples_directory = repository_root / "examples",
             .bosch_reference_directory = repository_root / "experiments/bosch_v10_reference",
             .bosch_fmu_library = cpssim::resolve_bundled_bosch_fmu(executable)});
        auto workbench = make_workbench(executable, repository_root);
        if (argc == 2) {
            workbench->open_project(std::filesystem::path{argv[1]});
        }
        auto* bridge = new cpssim::qt::QtWorkbenchBridge(std::move(workbench), &window);
        window.bind_workbench(bridge);
        window.show();
        return application.exec();
    } catch (const std::exception& error) {
        std::cerr << "cpssim_gui: " << error.what() << '\n';
        return 1;
    }
}
