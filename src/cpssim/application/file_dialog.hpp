/***
 * File: src/cpssim/application/file_dialog.hpp
 * Purpose: Declare the GUI-neutral boundary for user-selected files and directories.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include <filesystem>
#include <string>

namespace cpssim {

enum class FileDialogStatus {
    Selected,
    Cancelled,
    Failed,
};

struct FileDialogResult {
    FileDialogStatus status{FileDialogStatus::Cancelled};
    std::filesystem::path path;
    std::string diagnostic;

    static FileDialogResult selected(std::filesystem::path path) {
        return {.status = FileDialogStatus::Selected, .path = std::move(path), .diagnostic = {}};
    }
    static FileDialogResult cancelled() { return {}; }
    static FileDialogResult failed(std::string diagnostic) {
        return {
            .status = FileDialogStatus::Failed, .path = {}, .diagnostic = std::move(diagnostic)};
    }
};

/*** Hides platform dialog APIs behind one injectable application interface. ***/
class FileDialog {
  public:
    virtual ~FileDialog() = default;

    virtual FileDialogResult open_project(const std::filesystem::path& initial_directory) = 0;
    virtual FileDialogResult
    choose_project_parent(const std::filesystem::path& initial_directory) = 0;
    virtual FileDialogResult
    choose_trajectory_directory(const std::filesystem::path& initial_directory) = 0;
    virtual FileDialogResult open_run_plan(const std::filesystem::path& initial_directory) = 0;
    virtual FileDialogResult save_run_plan(const std::filesystem::path& suggested_path) = 0;
};

} // namespace cpssim
