/*** Implement disposable staging and explicit project layout persistence. ***/

#include "cpssim/application/gui_layout_store.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace cpssim {
namespace {

std::string read_text(const std::filesystem::path& path, std::string_view description) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"cannot read " + std::string{description} + " '" + path.string() +
                                 "'"};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error{"failed while reading " + std::string{description} + " '" +
                                 path.string() + "'"};
    }
    return contents.str();
}

void write_text_atomically(const std::filesystem::path& path, std::string_view contents) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        if (!output) {
            throw std::runtime_error{"cannot create temporary GUI layout '" + temporary.string() +
                                     "'"};
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error{"failed while writing temporary GUI layout '" +
                                     temporary.string() + "'"};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error{"cannot replace GUI layout '" + path.string() + "'"};
    }
}

std::filesystem::path allocate_temporary_directory(const std::filesystem::path& parent) {
    static std::atomic_uint64_t sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint64_t attempt = 0; attempt < 64; ++attempt) {
        const auto name = "cpssim-imgui-layout-" + std::to_string(timestamp) + '-' +
                          std::to_string(sequence.fetch_add(1)) + '-' + std::to_string(attempt);
        const auto candidate = parent / name;
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) {
            return candidate;
        }
        if (error && error != std::errc::file_exists) {
            throw std::runtime_error{"cannot create temporary GUI layout directory below '" +
                                     parent.string() + "'"};
        }
    }
    throw std::runtime_error{"cannot allocate a temporary GUI layout directory"};
}

} // namespace

GuiLayoutStore::GuiLayoutStore(const std::filesystem::path& default_layout_file,
                               const std::filesystem::path& temporary_parent)
    : default_layout_file_{std::filesystem::absolute(default_layout_file)},
      temporary_parent_{temporary_parent.empty() ? std::filesystem::temp_directory_path()
                                                 : std::filesystem::absolute(temporary_parent)} {
    if (!std::filesystem::is_regular_file(default_layout_file_)) {
        throw std::runtime_error{"default GUI layout is missing: " + default_layout_file_.string()};
    }
    if (!std::filesystem::is_directory(temporary_parent_)) {
        throw std::runtime_error{"temporary GUI layout parent is not a directory: " +
                                 temporary_parent_.string()};
    }
    default_settings_ = read_text(default_layout_file_, "default GUI layout");
    saved_settings_ = default_settings_;
    current_settings_ = default_settings_;
}

GuiLayoutStore::~GuiLayoutStore() { discard_temporary_file(); }

GuiLayoutActivation
GuiLayoutStore::activate(const std::optional<std::filesystem::path>& project_root) {
    discard_temporary_file();
    saved_settings_ = default_settings_;
    std::optional<std::string> diagnostic;
    if (project_root.has_value()) {
        const auto project_layout = *project_root / project_imgui_layout_filename;
        std::error_code error;
        const auto exists = std::filesystem::exists(project_layout, error);
        if (error) {
            diagnostic = "Default GUI layout used: cannot inspect project layout '" +
                         project_layout.string() + "'.";
        } else if (exists) {
            try {
                if (!std::filesystem::is_regular_file(project_layout)) {
                    throw std::runtime_error{"project GUI layout is not a regular file"};
                }
                saved_settings_ = read_text(project_layout, "project GUI layout");
            } catch (const std::exception& exception) {
                saved_settings_ = default_settings_;
                diagnostic = std::string{"Default GUI layout used: "} + exception.what();
            }
        }
    }
    current_settings_ = saved_settings_;
    return {.settings = current_settings_, .diagnostic = std::move(diagnostic)};
}

const std::string& GuiLayoutStore::restore_default() {
    record_current(default_settings_);
    return current_settings_;
}

void GuiLayoutStore::record_current(std::string settings) {
    current_settings_ = std::move(settings);
    if (!dirty()) {
        discard_temporary_file();
        return;
    }
    stage_temporary_file();
}

void GuiLayoutStore::save_to_project(const std::filesystem::path& project_root) {
    write_current_to_project(project_root);
    saved_settings_ = current_settings_;
    discard_temporary_file();
}

void GuiLayoutStore::write_current_to_project(const std::filesystem::path& project_root) const {
    if (!std::filesystem::is_directory(project_root)) {
        throw std::runtime_error{"project root is not a directory: " + project_root.string()};
    }
    const auto target = project_root / project_imgui_layout_filename;
    if (current_settings_ == default_settings_) {
        std::error_code error;
        const auto exists = std::filesystem::exists(target, error);
        if (!error && exists && !std::filesystem::is_regular_file(target)) {
            throw std::runtime_error{"project GUI layout is not a regular file: " +
                                     target.string()};
        }
        if (!error && exists) {
            static_cast<void>(std::filesystem::remove(target, error));
        }
        if (error) {
            throw std::runtime_error{"cannot remove project GUI layout '" + target.string() + "'"};
        }
        return;
    }
    write_text_atomically(target, current_settings_);
}

void GuiLayoutStore::stage_temporary_file() {
    if (!temporary_directory_.has_value()) {
        const auto directory = allocate_temporary_directory(temporary_parent_);
        temporary_directory_ = directory;
        temporary_file_ = directory / project_imgui_layout_filename;
    }
    const auto staging_file = temporary_file_.value_or(std::filesystem::path{});
    if (staging_file.empty()) {
        throw std::logic_error{"temporary GUI layout file was not initialized"};
    }
    write_text_atomically(staging_file, current_settings_);
}

void GuiLayoutStore::discard_temporary_file() noexcept {
    std::error_code ignored;
    if (temporary_file_.has_value()) {
        std::filesystem::remove(*temporary_file_, ignored);
    }
    if (temporary_directory_.has_value()) {
        std::filesystem::remove(*temporary_directory_, ignored);
    }
    temporary_file_.reset();
    temporary_directory_.reset();
}

} // namespace cpssim
