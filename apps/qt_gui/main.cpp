/*** Start the native Qt Widgets CPSSim frontend. ***/
#include "apps/qt_gui/main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == "--help") {
            std::cout << "Usage: cpssim_qt_gui [project.json]\n";
            return 0;
        }
    }
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("CPSSim");
    QCoreApplication::setApplicationName("CPSSim Qt GUI");
    cpssim::qt::QtMainWindow window;
    window.show();
    return application.exec();
}
