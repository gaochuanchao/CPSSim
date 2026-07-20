/*** GUI-only portable native dialog adapter. ***/

#pragma once

#include "cpssim/application/file_dialog.hpp"

namespace cpssim::gui {

class NativeFileDialog final : public FileDialog {
  public:
    FileDialogResult open_project(const std::filesystem::path& initial_directory) override;
    FileDialogResult choose_project_parent(const std::filesystem::path& initial_directory) override;
    FileDialogResult
    choose_trajectory_directory(const std::filesystem::path& initial_directory) override;
    FileDialogResult open_run_plan(const std::filesystem::path& initial_directory) override;
    FileDialogResult save_run_plan(const std::filesystem::path& suggested_path) override;
    FileDialogResult
    choose_results_directory(const std::filesystem::path& initial_directory) override;

  private:
    static FileDialogResult select_folder(std::string title,
                                          const std::filesystem::path& initial_directory);
};

} // namespace cpssim::gui
