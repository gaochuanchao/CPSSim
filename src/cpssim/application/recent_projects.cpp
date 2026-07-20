/*** Persist recents separately from project files and tolerate damaged preferences. ***/

#include "cpssim/application/recent_projects.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace cpssim {
namespace {

using Json = nlohmann::json;
constexpr std::uint32_t recent_schema_version = 1;

std::filesystem::path normalize(const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::invalid_argument{"recent project path must not be empty"};
    }
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path).lexically_normal();
}

bool same_path(const std::filesystem::path& left, const std::filesystem::path& right) {
    return normalize(left) == normalize(right);
}

} // namespace

void RecentProjects::add(const std::filesystem::path& project_file) {
    const auto normalized = normalize(project_file);
    std::erase_if(entries_, [&](const auto& entry) {
        return same_path(entry.project_file, normalized);
    });
    entries_.insert(entries_.begin(),
                    {.project_file = normalized,
                     .available = std::filesystem::is_regular_file(normalized)});
    if (entries_.size() > maximum_recent_projects) {
        entries_.resize(maximum_recent_projects);
    }
}

void RecentProjects::remove(const std::filesystem::path& project_file) {
    std::erase_if(entries_, [&](const auto& entry) {
        return same_path(entry.project_file, project_file);
    });
}

void RecentProjects::refresh_availability() {
    for (auto& entry : entries_) {
        entry.available = std::filesystem::is_regular_file(entry.project_file);
    }
}

std::filesystem::path default_gui_preferences_file() {
#if defined(_WIN32)
    const char* base = std::getenv("APPDATA");
    return base != nullptr ? std::filesystem::path{base} / "CPSSim" / "preferences.json"
                           : std::filesystem::path{"CPSSim/preferences.json"};
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home != nullptr
               ? std::filesystem::path{home} / "Library/Application Support/CPSSim/preferences.json"
               : std::filesystem::path{"CPSSim/preferences.json"};
#else
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && *config != '\0') {
        return std::filesystem::path{config} / "cpssim/preferences.json";
    }
    const char* home = std::getenv("HOME");
    return home != nullptr ? std::filesystem::path{home} / ".config/cpssim/preferences.json"
                           : std::filesystem::path{".cpssim/preferences.json"};
#endif
}

RecentProjectsLoadResult load_recent_projects(const std::filesystem::path& preferences_file) {
    RecentProjectsLoadResult result;
    if (!std::filesystem::exists(preferences_file)) {
        return result;
    }
    try {
        std::ifstream input{preferences_file};
        if (!input) {
            throw std::runtime_error{"cannot open preferences file"};
        }
        Json document;
        input >> document;
        if (!document.is_object() || document.size() != 2 ||
            document.value("schema_version", 0U) != recent_schema_version ||
            !document.contains("recent_projects") || !document["recent_projects"].is_array()) {
            throw std::invalid_argument{"unsupported recent-project preferences schema"};
        }
        std::vector<std::filesystem::path> loaded;
        for (const auto& value : document["recent_projects"]) {
            if (!value.is_string()) {
                throw std::invalid_argument{"recent project paths must be strings"};
            }
            loaded.push_back(value.get<std::string>());
            if (loaded.size() == maximum_recent_projects) {
                break;
            }
        }
        for (auto entry = loaded.rbegin(); entry != loaded.rend(); ++entry) {
            result.recent.add(*entry);
        }
    } catch (const std::exception& error) {
        result.recent = {};
        result.diagnostic = std::string{"Recent-project defaults used: "} + error.what();
    }
    return result;
}

void save_recent_projects(const std::filesystem::path& preferences_file,
                          const RecentProjects& recent) {
    Json paths = Json::array();
    for (const auto& entry : recent.entries()) {
        paths.push_back(normalize(entry.project_file).generic_string());
    }
    const auto document =
        Json{{"schema_version", recent_schema_version}, {"recent_projects", std::move(paths)}};
    std::filesystem::create_directories(preferences_file.parent_path());
    auto temporary = preferences_file;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::trunc};
        if (!output) {
            throw std::runtime_error{"cannot write GUI preferences"};
        }
        output << document.dump(2) << '\n';
        if (!output) {
            throw std::runtime_error{"cannot write GUI preferences"};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, preferences_file, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error{"cannot replace GUI preferences"};
    }
}

} // namespace cpssim
