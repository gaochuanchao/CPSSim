/***
 * File: src/cpssim/application/project/project.cpp
 * Purpose: Implement strict, contained, and safely written project persistence.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#include "cpssim/application/project/project.hpp"

#include "cpssim/config/json_config.hpp"
#include "cpssim/config/json_run_plan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace cpssim {
namespace {

using Json = nlohmann::json;

[[noreturn]] void fail_project(std::string path, std::string message) {
    throw std::invalid_argument{"project " + std::move(path) + ": " + std::move(message)};
}

void require_object(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        fail_project(path, "must be a JSON object");
    }
}

void require_only_fields(const Json& object, std::initializer_list<std::string_view> allowed,
                         const std::string& path) {
    require_object(object, path);
    for (auto field = object.begin(); field != object.end(); ++field) {
        const auto known = std::any_of(allowed.begin(), allowed.end(),
                                       [&field](const auto name) { return field.key() == name; });
        if (!known) {
            fail_project(path + '.' + field.key(), "unknown field");
        }
    }
}

const Json& require_field(const Json& object, std::string_view name, const std::string& path) {
    const auto found = object.find(name);
    if (found == object.end()) {
        fail_project(path + '.' + std::string{name}, "missing required field");
    }
    return *found;
}

std::uint32_t read_schema_version(const Json& value, const std::string& path,
                                  std::uint32_t expected) {
    std::uint64_t version = 0;
    if (value.is_number_unsigned()) {
        version = value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        const auto signed_version = value.get<std::int64_t>();
        if (signed_version < 0) {
            fail_project(path, "must be a nonnegative integer");
        }
        version = static_cast<std::uint64_t>(signed_version);
    } else {
        fail_project(path, "must be a nonnegative integer");
    }
    if (version != expected) {
        fail_project(path, "unsupported version " + std::to_string(version) + "; expected " +
                               std::to_string(expected));
    }
    return static_cast<std::uint32_t>(version);
}

std::string read_nonempty_string(const Json& value, const std::string& path) {
    if (!value.is_string()) {
        fail_project(path, "must be a string");
    }
    auto result = value.get<std::string>();
    if (result.empty()) {
        fail_project(path, "must not be empty");
    }
    return result;
}

void validate_project_name(const std::string& name) {
    if (name.empty()) {
        fail_project("$.name", "must not be empty");
    }
    const std::filesystem::path path{name};
    if (path.is_absolute() || path.has_parent_path() || name == "." || name == ".." ||
        name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        fail_project("$.name", "must be a single directory name");
    }
}

std::filesystem::path validate_internal_path(const std::filesystem::path& path,
                                             const std::string& field_path) {
    const auto text = path.generic_string();
    const auto has_windows_drive = text.size() >= 2 &&
                                   std::isalpha(static_cast<unsigned char>(text[0])) != 0 &&
                                   text[1] == ':';
    if (path.empty() || path.is_absolute() || path.has_root_path() || has_windows_drive) {
        fail_project(field_path, "must be a relative project-owned path");
    }
    if (text.find('\\') != std::string::npos) {
        fail_project(field_path, "must use portable forward-slash separators");
    }

    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        fail_project(field_path, "must name a project-owned file");
    }
    const auto first = normalized.begin();
    if (first != normalized.end() && *first == "..") {
        fail_project(field_path, "must not escape the project directory");
    }
    return normalized;
}

void validate_metadata(const ProjectMetadata& metadata) {
    if (metadata.schema_version != current_project_schema_version) {
        fail_project("$.schema_version",
                     "unsupported version " + std::to_string(metadata.schema_version) +
                         "; expected " + std::to_string(current_project_schema_version));
    }
    validate_project_name(metadata.name);
    if (metadata.scenario_kind.empty()) {
        fail_project("$.scenario.kind", "must not be empty");
    }
    static_cast<void>(validate_internal_path(metadata.system_file, "$.system_file"));
    static_cast<void>(validate_internal_path(metadata.workspace_file, "$.workspace_file"));
    static_cast<void>(validate_internal_path(metadata.default_run_plan, "$.default_run_plan"));
}

bool path_is_within(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto candidate_part = candidate.begin();
    for (auto root_part = root.begin(); root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *candidate_part != *root_part) {
            return false;
        }
    }
    return true;
}

std::filesystem::path resolve_internal_path(const std::filesystem::path& root,
                                            const std::filesystem::path& relative,
                                            const std::string& field_path) {
    const auto normalized = validate_internal_path(relative, field_path);
    const auto resolved_root = std::filesystem::weakly_canonical(root);
    const auto resolved = std::filesystem::weakly_canonical(resolved_root / normalized);
    if (!path_is_within(resolved_root, resolved)) {
        fail_project(field_path, "resolves outside the project directory");
    }
    return resolved;
}

std::string read_text_file(const std::filesystem::path& path, const std::string& field_path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"project " + field_path + ": cannot open '" + path.string() +
                                 "' for reading"};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad()) {
        throw std::runtime_error{"project " + field_path + ": failed while reading '" +
                                 path.string() + "'"};
    }
    return contents.str();
}

void write_text_atomically(const std::filesystem::path& path, std::string_view contents) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::trunc};
        if (!output) {
            throw std::runtime_error{"cannot open temporary project file '" + temporary.string() +
                                     "' for writing"};
        }
        output << contents;
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error{"failed while writing temporary project file '" +
                                     temporary.string() + "'"};
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error{"cannot replace project file '" + path.string() + "'"};
    }
}

template <typename Loader> auto load_component(const std::string& field_path, Loader&& loader) {
    try {
        return std::forward<Loader>(loader)();
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument{"project " + field_path + ": " + error.what()};
    } catch (const std::runtime_error& error) {
        throw std::runtime_error{"project " + field_path + ": " + error.what()};
    }
}

} // namespace

ProjectContext::ProjectContext(std::filesystem::path root, ProjectMetadata metadata,
                               RunPlan default_run_plan, ProjectWorkspace workspace,
                               std::unique_ptr<GuiSimulationSession> session)
    : root_{std::move(root)}, metadata_{std::move(metadata)},
      default_run_plan_{std::move(default_run_plan)}, workspace_{workspace},
      session_{std::move(session)} {
    if (session_ == nullptr) {
        throw std::invalid_argument{"project context session must not be empty"};
    }
}

std::string serialize_project_metadata_json(const ProjectMetadata& metadata) {
    validate_metadata(metadata);
    const Json document{{"default_run_plan", metadata.default_run_plan.generic_string()},
                        {"name", metadata.name},
                        {"scenario", Json{{"kind", metadata.scenario_kind}}},
                        {"schema_version", metadata.schema_version},
                        {"system_file", metadata.system_file.generic_string()},
                        {"workspace_file", metadata.workspace_file.generic_string()}};
    return document.dump(2) + '\n';
}

ProjectMetadata parse_project_metadata_json(std::string_view json_text) {
    Json document;
    try {
        document = Json::parse(json_text);
    } catch (const Json::parse_error& error) {
        fail_project("$ (byte " + std::to_string(error.byte) + ')', "malformed JSON");
    }

    require_only_fields(
        document,
        {"schema_version", "name", "system_file", "workspace_file", "default_run_plan", "scenario"},
        "$");
    const std::string root_path{"$"};
    const auto& scenario = require_field(document, "scenario", root_path);
    require_only_fields(scenario, {"kind"}, "$.scenario");

    ProjectMetadata metadata{
        .schema_version = read_schema_version(require_field(document, "schema_version", "$"),
                                              "$.schema_version", current_project_schema_version),
        .name = read_nonempty_string(require_field(document, "name", "$"), "$.name"),
        .system_file =
            read_nonempty_string(require_field(document, "system_file", "$"), "$.system_file"),
        .workspace_file = read_nonempty_string(require_field(document, "workspace_file", "$"),
                                               "$.workspace_file"),
        .default_run_plan = read_nonempty_string(require_field(document, "default_run_plan", "$"),
                                                 "$.default_run_plan"),
        .scenario_kind =
            read_nonempty_string(require_field(scenario, "kind", "$.scenario"), "$.scenario.kind")};
    validate_metadata(metadata);
    return metadata;
}

std::string serialize_project_workspace_json(const ProjectWorkspace& workspace) {
    if (workspace.schema_version != current_project_workspace_schema_version) {
        fail_project("workspace $.schema_version",
                     "unsupported version " + std::to_string(workspace.schema_version) +
                         "; expected " + std::to_string(current_project_workspace_schema_version));
    }
    return Json{{"schema_version", workspace.schema_version}}.dump(2) + '\n';
}

ProjectWorkspace parse_project_workspace_json(std::string_view json_text) {
    Json document;
    try {
        document = Json::parse(json_text);
    } catch (const Json::parse_error& error) {
        fail_project("workspace $ (byte " + std::to_string(error.byte) + ')', "malformed JSON");
    }
    require_only_fields(document, {"schema_version"}, "workspace $");
    return ProjectWorkspace{.schema_version = read_schema_version(
                                require_field(document, "schema_version", "workspace $"),
                                "workspace $.schema_version",
                                current_project_workspace_schema_version)};
}

std::unique_ptr<ProjectContext>
make_project_context(std::filesystem::path root, ProjectMetadata metadata, ExperimentConfig system,
                     RunPlan default_run_plan, ProjectWorkspace workspace) {
    validate_metadata(metadata);
    auto session =
        std::make_unique<GuiSimulationSession>(std::move(system), default_run_plan.stop_tick());
    if (!session->replace_draft(default_run_plan) || !session->apply_draft()) {
        throw std::runtime_error{"project default run plan could not construct a GUI session"};
    }
    return std::make_unique<ProjectContext>(std::filesystem::weakly_canonical(root),
                                            std::move(metadata), std::move(default_run_plan),
                                            workspace, std::move(session));
}

std::unique_ptr<ProjectContext> create_project(const ProjectCreationRequest& request) {
    if (request.parent_directory.empty()) {
        fail_project("$.root", "parent directory must not be empty");
    }
    ProjectMetadata metadata{.name = request.name, .scenario_kind = request.scenario_kind};
    validate_metadata(metadata);

    const auto system_json = serialize_experiment_config_json(request.system);
    const auto run_plan_json = serialize_run_plan_json(request.system, request.default_run_plan);
    const auto workspace_json = serialize_project_workspace_json(request.workspace);
    const auto project_json = serialize_project_metadata_json(metadata);

    const auto parent = std::filesystem::absolute(request.parent_directory).lexically_normal();
    if (std::filesystem::exists(parent) && !std::filesystem::is_directory(parent)) {
        throw std::runtime_error{"project parent is not a directory: " + parent.string()};
    }
    std::filesystem::create_directories(parent);
    const auto root = parent / request.name;
    if (std::filesystem::exists(root) || !std::filesystem::create_directory(root)) {
        throw std::runtime_error{"project directory already exists: " + root.string()};
    }

    std::filesystem::create_directory(root / "run-plans");
    std::filesystem::create_directory(root / "results");

    auto context = make_project_context(root, metadata, request.system, request.default_run_plan,
                                        request.workspace);
    write_text_atomically(root / metadata.system_file, system_json);
    write_text_atomically(root / metadata.default_run_plan, run_plan_json);
    write_text_atomically(root / metadata.workspace_file, workspace_json);
    write_text_atomically(root / "project.json", project_json);
    return context;
}

std::unique_ptr<ProjectContext> load_project(const std::filesystem::path& project_file) {
    const auto project_path = std::filesystem::absolute(project_file).lexically_normal();
    const auto root = std::filesystem::weakly_canonical(project_path.parent_path());
    const auto metadata = parse_project_metadata_json(read_text_file(project_path, "$.project"));

    const auto system_path = resolve_internal_path(root, metadata.system_file, "$.system_file");
    const auto run_plan_path =
        resolve_internal_path(root, metadata.default_run_plan, "$.default_run_plan");
    const auto workspace_path =
        resolve_internal_path(root, metadata.workspace_file, "$.workspace_file");
    const auto results_path = resolve_internal_path(root, "results", "$.results");
    if (!std::filesystem::is_directory(results_path)) {
        throw std::runtime_error{"project $.results: required directory is missing"};
    }

    auto system =
        load_component("$.system_file", [&] { return load_experiment_config(system_path); });
    auto default_run_plan =
        load_component("$.default_run_plan", [&] { return load_run_plan(run_plan_path, system); });
    auto workspace = load_component("$.workspace_file", [&] {
        return parse_project_workspace_json(read_text_file(workspace_path, "$.workspace_file"));
    });

    return make_project_context(root, metadata, std::move(system), std::move(default_run_plan),
                                workspace);
}

void save_project(const ProjectContext& project) {
    const auto& metadata = project.metadata();
    validate_metadata(metadata);
    const auto system_path =
        resolve_internal_path(project.root(), metadata.system_file, "$.system_file");
    const auto run_plan_path =
        resolve_internal_path(project.root(), metadata.default_run_plan, "$.default_run_plan");
    const auto workspace_path =
        resolve_internal_path(project.root(), metadata.workspace_file, "$.workspace_file");

    const auto system_json = serialize_experiment_config_json(project.session().config());
    const auto run_plan_json =
        serialize_run_plan_json(project.session().config(), project.default_run_plan());
    const auto workspace_json = serialize_project_workspace_json(project.workspace());
    const auto project_json = serialize_project_metadata_json(metadata);

    write_text_atomically(system_path, system_json);
    write_text_atomically(run_plan_path, run_plan_json);
    write_text_atomically(workspace_path, workspace_json);
    write_text_atomically(project.root() / "project.json", project_json);
}

} // namespace cpssim
