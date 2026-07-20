/*** Own the default, temporary, and project-scoped Dear ImGui layout files. ***/

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace cpssim {

inline constexpr std::string_view project_imgui_layout_filename = "imgui.ini";

struct GuiLayoutActivation {
    std::string settings;
    std::optional<std::string> diagnostic;
};

/***
 * Keeps the tracked default read-only from the application's perspective.
 * Edits are staged in one disposable temporary file and reach a project only
 * through an explicit project save operation.
 ***/
class GuiLayoutStore {
  public:
    explicit GuiLayoutStore(const std::filesystem::path& default_layout_file,
                            const std::filesystem::path& temporary_parent = {});
    ~GuiLayoutStore();

    GuiLayoutStore(const GuiLayoutStore&) = delete;
    GuiLayoutStore& operator=(const GuiLayoutStore&) = delete;
    GuiLayoutStore(GuiLayoutStore&&) = delete;
    GuiLayoutStore& operator=(GuiLayoutStore&&) = delete;

    GuiLayoutActivation
    activate(const std::optional<std::filesystem::path>& project_root = std::nullopt);
    const std::string& restore_default();
    void record_current(std::string settings);

    void save_to_project(const std::filesystem::path& project_root);
    void write_current_to_project(const std::filesystem::path& project_root) const;

    const std::string& current_settings() const noexcept { return current_settings_; }
    bool dirty() const noexcept { return current_settings_ != saved_settings_; }
    const std::optional<std::filesystem::path>& temporary_file() const noexcept {
        return temporary_file_;
    }

  private:
    void stage_temporary_file();
    void discard_temporary_file() noexcept;

    std::filesystem::path default_layout_file_;
    std::filesystem::path temporary_parent_;
    std::string default_settings_;
    std::string saved_settings_;
    std::string current_settings_;
    std::optional<std::filesystem::path> temporary_directory_;
    std::optional<std::filesystem::path> temporary_file_;
};

} // namespace cpssim
