/*** Verify bounded, normalized, user-preference recent project history. ***/

#include "cpssim/application/recent_projects.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{0};
        root_ = std::filesystem::temp_directory_path() /
                ("cpssim-recent-test-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
                 std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directory(root_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
};

void touch(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream{path} << "{}\n";
}

} // namespace

TEST_CASE("recent projects order, deduplicate, limit, persist, and mark missing entries",
          "[project][recent]") {
    TemporaryDirectory temporary;
    cpssim::RecentProjects recent;
    for (int index = 0; index < 10; ++index) {
        const auto path = temporary.root() / std::to_string(index) / "project.json";
        touch(path);
        recent.add(path);
    }
    REQUIRE(recent.entries().size() == cpssim::maximum_recent_projects);
    REQUIRE((recent.entries().front().project_file.parent_path().filename() == "9"));

    const auto reopened = temporary.root() / "5" / "." / "project.json";
    recent.add(reopened);
    REQUIRE(recent.entries().size() == cpssim::maximum_recent_projects);
    REQUIRE((recent.entries().front().project_file.parent_path().filename() == "5"));

    std::filesystem::remove(recent.entries().front().project_file);
    recent.refresh_availability();
    REQUIRE_FALSE(recent.entries().front().available);

    const auto preferences = temporary.root() / "preferences.json";
    cpssim::save_recent_projects(preferences, recent);
    const auto loaded = cpssim::load_recent_projects(preferences);
    REQUIRE_FALSE(loaded.diagnostic.has_value());
    REQUIRE((loaded.recent.entries() == recent.entries()));

    const auto removed = loaded.recent.entries().front().project_file;
    auto mutable_recent = loaded.recent;
    mutable_recent.remove(removed);
    REQUIRE(mutable_recent.entries().size() == cpssim::maximum_recent_projects - 1);
}

TEST_CASE("malformed recent preferences use safe defaults with a diagnostic",
          "[project][recent][fallback]") {
    TemporaryDirectory temporary;
    const auto preferences = temporary.root() / "preferences.json";
    std::ofstream{preferences} << "{";
    const auto loaded = cpssim::load_recent_projects(preferences);
    REQUIRE(loaded.recent.entries().empty());
    REQUIRE(loaded.diagnostic.has_value());
}
