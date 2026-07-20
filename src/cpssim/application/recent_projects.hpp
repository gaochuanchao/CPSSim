/*** GUI user-preference persistence for a bounded recent-project list. ***/

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

inline constexpr std::size_t maximum_recent_projects = 8;

struct RecentProjectEntry {
    std::filesystem::path project_file;
    bool available{false};

    bool operator==(const RecentProjectEntry&) const = default;
};

class RecentProjects {
  public:
    const std::vector<RecentProjectEntry>& entries() const { return entries_; }
    void add(const std::filesystem::path& project_file);
    void remove(const std::filesystem::path& project_file);
    void refresh_availability();

  private:
    std::vector<RecentProjectEntry> entries_;

    friend struct RecentProjectsLoadResult;
};

struct RecentProjectsLoadResult {
    RecentProjects recent;
    std::optional<std::string> diagnostic;
};

std::filesystem::path default_gui_preferences_file();
RecentProjectsLoadResult load_recent_projects(const std::filesystem::path& preferences_file);
void save_recent_projects(const std::filesystem::path& preferences_file,
                          const RecentProjects& recent);

} // namespace cpssim
