/*** Implement native/platform-neutral dialogs through portable-file-dialogs. ***/

#include "native_file_dialog.hpp"

#include "portable-file-dialogs.h"

#include <exception>
#include <string>
#include <vector>

namespace cpssim::gui {
namespace {

FileDialogResult unavailable() {
    return FileDialogResult::failed(
        "No supported platform file-dialog service is available on this desktop.");
}

FileDialogResult open_json(std::string title, const std::filesystem::path& initial_directory) {
    if (!pfd::settings::available()) {
        return unavailable();
    }
    try {
        const auto paths =
            pfd::open_file(title, initial_directory.string(), {"JSON files", "*.json"}).result();
        return paths.empty() ? FileDialogResult::cancelled()
                             : FileDialogResult::selected(paths.front());
    } catch (const std::exception& error) {
        return FileDialogResult::failed(error.what());
    }
}

} // namespace

FileDialogResult NativeFileDialog::open_project(const std::filesystem::path& initial_directory) {
    return open_json("Open CPSSim Project", initial_directory);
}

FileDialogResult
NativeFileDialog::choose_project_parent(const std::filesystem::path& initial_directory) {
    return select_folder("Choose Project Parent Directory", initial_directory);
}

FileDialogResult
NativeFileDialog::choose_trajectory_directory(const std::filesystem::path& initial_directory) {
    return select_folder("Choose Bosch Trajectory Directory", initial_directory);
}

FileDialogResult NativeFileDialog::open_run_plan(const std::filesystem::path& initial_directory) {
    return open_json("Load CPSSim Run Plan", initial_directory);
}

FileDialogResult NativeFileDialog::save_run_plan(const std::filesystem::path& suggested_path) {
    if (!pfd::settings::available()) {
        return unavailable();
    }
    try {
        const auto path = pfd::save_file("Save CPSSim Run Plan", suggested_path.string(),
                                         {"JSON files", "*.json"})
                              .result();
        return path.empty() ? FileDialogResult::cancelled() : FileDialogResult::selected(path);
    } catch (const std::exception& error) {
        return FileDialogResult::failed(error.what());
    }
}

FileDialogResult
NativeFileDialog::choose_results_directory(const std::filesystem::path& initial_directory) {
    return select_folder("Choose Run Results Directory", initial_directory);
}

FileDialogResult NativeFileDialog::select_folder(std::string title,
                                                 const std::filesystem::path& initial_directory) {
    if (!pfd::settings::available()) {
        return unavailable();
    }
    try {
        const auto path = pfd::select_folder(title, initial_directory.string()).result();
        return path.empty() ? FileDialogResult::cancelled() : FileDialogResult::selected(path);
    } catch (const std::exception& error) {
        return FileDialogResult::failed(error.what());
    }
}

} // namespace cpssim::gui
