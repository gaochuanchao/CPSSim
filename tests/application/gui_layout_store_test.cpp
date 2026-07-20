/*** Verify immutable defaults, disposable staging, and explicit project layout saves. ***/

#include "cpssim/application/gui_layout_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace cpssim;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        const auto suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
            std::to_string(sequence.fetch_add(1));
        root_ = std::filesystem::temp_directory_path() / ("cpssim-layout-test-" + suffix);
        if (!std::filesystem::create_directory(root_)) {
            throw std::runtime_error{"could not create layout-test directory"};
        }
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
};

void write_text(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output << contents;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

TEST_CASE("GUI layout edits stage without changing the default", "[project][layout]") {
    TemporaryDirectory temporary;
    const auto defaults = temporary.root() / "default.ini";
    write_text(defaults, "default-layout\n");
    GuiLayoutStore store{defaults, temporary.root()};

    const auto activated = store.activate();
    REQUIRE(activated.settings == "default-layout\n");
    REQUIRE_FALSE(store.dirty());
    REQUIRE_FALSE(store.temporary_file().has_value());

    store.record_current("adjusted-layout\n");
    REQUIRE(store.dirty());
    REQUIRE(store.temporary_file().has_value());
    REQUIRE(std::filesystem::is_regular_file(*store.temporary_file()));
    REQUIRE(read_text(defaults) == "default-layout\n");
}

TEST_CASE("unsaved GUI layout staging is deleted on context replacement and destruction",
          "[project][layout][cleanup]") {
    TemporaryDirectory temporary;
    const auto defaults = temporary.root() / "default.ini";
    const auto project = temporary.root() / "project";
    std::filesystem::create_directory(project);
    write_text(defaults, "default-layout\n");

    std::filesystem::path staged;
    {
        GuiLayoutStore store{defaults, temporary.root()};
        store.record_current("first-adjustment\n");
        staged = *store.temporary_file();
        REQUIRE(std::filesystem::exists(staged));
        store.activate(project);
        REQUIRE_FALSE(std::filesystem::exists(staged));

        store.record_current("second-adjustment\n");
        staged = *store.temporary_file();
        REQUIRE(std::filesystem::exists(staged));
    }
    REQUIRE_FALSE(std::filesystem::exists(staged));
    REQUIRE_FALSE(std::filesystem::exists(project / project_imgui_layout_filename));
}

TEST_CASE("saved GUI layouts reopen with projects and restore removes the override",
          "[project][layout][round-trip]") {
    TemporaryDirectory temporary;
    const auto defaults = temporary.root() / "default.ini";
    const auto project = temporary.root() / "project";
    std::filesystem::create_directory(project);
    write_text(defaults, "default-layout\n");
    GuiLayoutStore store{defaults, temporary.root()};

    store.activate(project);
    store.record_current("project-layout\n");
    store.save_to_project(project);
    REQUIRE_FALSE(store.dirty());
    REQUIRE_FALSE(store.temporary_file().has_value());
    REQUIRE(read_text(project / project_imgui_layout_filename) == "project-layout\n");

    REQUIRE(store.activate(project).settings == "project-layout\n");
    REQUIRE(store.restore_default() == "default-layout\n");
    REQUIRE(store.dirty());
    store.save_to_project(project);
    REQUIRE_FALSE(std::filesystem::exists(project / project_imgui_layout_filename));
    REQUIRE(store.activate(project).settings == "default-layout\n");
}

TEST_CASE("Save As can publish staged GUI layout without committing the source project",
          "[project][layout][save-as]") {
    TemporaryDirectory temporary;
    const auto defaults = temporary.root() / "default.ini";
    const auto source = temporary.root() / "source";
    const auto copy = temporary.root() / "copy";
    std::filesystem::create_directory(source);
    std::filesystem::create_directory(copy);
    write_text(defaults, "default-layout\n");
    GuiLayoutStore store{defaults, temporary.root()};

    store.activate(source);
    store.record_current("staged-layout\n");
    REQUIRE(store.temporary_file().has_value());
    const auto staged = store.temporary_file().value_or(std::filesystem::path{});
    store.write_current_to_project(copy);

    REQUIRE(store.dirty());
    REQUIRE(std::filesystem::exists(staged));
    REQUIRE_FALSE(std::filesystem::exists(source / project_imgui_layout_filename));
    REQUIRE(read_text(copy / project_imgui_layout_filename) == "staged-layout\n");
}

TEST_CASE("invalid optional project GUI layout falls back without changing the project",
          "[project][layout][fallback]") {
    TemporaryDirectory temporary;
    const auto defaults = temporary.root() / "default.ini";
    const auto project = temporary.root() / "project";
    std::filesystem::create_directory(project);
    std::filesystem::create_directory(project / project_imgui_layout_filename);
    write_text(defaults, "default-layout\n");
    GuiLayoutStore store{defaults, temporary.root()};

    const auto activated = store.activate(project);
    REQUIRE(activated.settings == "default-layout\n");
    REQUIRE(activated.diagnostic.has_value());
    REQUIRE(std::filesystem::is_directory(project / project_imgui_layout_filename));
}

} // namespace
