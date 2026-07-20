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
    if (metadata.scenario_file.has_value()) {
        static_cast<void>(validate_internal_path(*metadata.scenario_file, "$.scenario.file"));
    }
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

std::filesystem::path resolve_internal_path(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const std::filesystem::path& root, const std::filesystem::path& relative,
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

const RunPlan& accepted_project_plan(const ProjectContext& project) {
    const auto* active = project.session().active_plan();
    return active != nullptr ? *active : project.default_run_plan();
}

} // namespace

ProjectContext::ProjectContext(std::filesystem::path root, ProjectMetadata metadata,
                               RunPlan default_run_plan, ProjectWorkspace workspace,
                               std::unique_ptr<GuiSimulationSession> session,
                               ProjectRuntimeInputs runtime_inputs,
                               std::optional<std::string> workspace_diagnostic)
    : root_{std::move(root)}, metadata_{std::move(metadata)},
      default_run_plan_{std::move(default_run_plan)}, workspace_{std::move(workspace)},
      session_{std::move(session)}, runtime_inputs_{std::move(runtime_inputs)},
      workspace_diagnostic_{std::move(workspace_diagnostic)} {
    if (session_ == nullptr) {
        throw std::invalid_argument{"project context session must not be empty"};
    }
    normalize_workspace_state(workspace_);
}

void ProjectContext::set_workspace(ProjectWorkspace workspace) {
    normalize_workspace_state(workspace);
    workspace_ = std::move(workspace);
}

std::string serialize_project_metadata_json(const ProjectMetadata& metadata) {
    validate_metadata(metadata);
    Json scenario{{"kind", metadata.scenario_kind}};
    if (metadata.scenario_file.has_value()) {
        scenario["file"] = metadata.scenario_file->generic_string();
    }
    const Json document{{"default_run_plan", metadata.default_run_plan.generic_string()},
                        {"name", metadata.name},
                        {"scenario", std::move(scenario)},
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
    require_only_fields(scenario, {"kind", "file"}, "$.scenario");

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
        .scenario_file = std::nullopt,
        .scenario_kind =
            read_nonempty_string(require_field(scenario, "kind", "$.scenario"), "$.scenario.kind")};
    if (const auto scenario_file = scenario.find("file"); scenario_file != scenario.end()) {
        metadata.scenario_file = read_nonempty_string(*scenario_file, "$.scenario.file");
    }
    validate_metadata(metadata);
    return metadata;
}

std::string serialize_project_workspace_json(const ProjectWorkspace& workspace) {
    if (workspace.schema_version != current_project_workspace_schema_version) {
        fail_project("workspace $.schema_version",
                     "unsupported version " + std::to_string(workspace.schema_version) +
                         "; expected " + std::to_string(current_project_workspace_schema_version));
    }
    const auto theme = workspace.theme == GuiTheme::Light ? "light" : "dark";
    const auto analysis_tab = [&] {
        switch (workspace.active_analysis_tab) {
        case GuiAnalysisTab::Architecture:
            return "architecture";
        case GuiAnalysisTab::Timeline:
            return "timeline";
        case GuiAnalysisTab::Signals:
            return "signals";
        }
        return "architecture";
    }();
    const auto resource_tab = workspace.active_resource_tab == GuiResourceTab::Utilization
                                  ? "utilization"
                                  : "resource_state";
    const auto event_type = [](EventType type) {
        switch (type) {
        case EventType::JobRelease:
            return "job_release";
        case EventType::JobStart:
            return "job_start";
        case EventType::JobPreempt:
            return "job_preempt";
        case EventType::JobResume:
            return "job_resume";
        case EventType::JobFinish:
            return "job_finish";
        case EventType::DeadlineMiss:
            return "deadline_miss";
        case EventType::MessageSend:
            return "message_send";
        case EventType::MessageDelivery:
            return "message_delivery";
        }
        return "job_release";
    };
    Json filters{{"text", workspace.event_filters.text}};
    if (workspace.event_filters.type.has_value()) {
        filters["type"] = event_type(*workspace.event_filters.type);
    }
    if (workspace.event_filters.task.has_value()) {
        filters["task"] = workspace.event_filters.task->value();
    }
    if (workspace.event_filters.resource.has_value()) {
        filters["resource"] = workspace.event_filters.resource->value();
    }
    if (workspace.event_filters.vehicle.has_value()) {
        filters["vehicle"] = workspace.event_filters.vehicle->value();
    }
    Json signals = Json::array();
    for (const auto& signal : workspace.selected_signals) {
        const auto scalar_type = signal.scalar_type == GuiSignalScalarType::Real      ? "real"
                                 : signal.scalar_type == GuiSignalScalarType::Integer ? "integer"
                                                                                      : "boolean";
        signals.push_back({{"source_name", signal.source_name}, {"type", scalar_type}});
    }
    const auto& panels = workspace.panels;
    const auto& columns = workspace.event_columns;
    const Json document{{"active_tabs", {{"analysis", analysis_tab}, {"resources", resource_tab}}},
                        {"event_columns",
                         {{"cause", columns.cause},
                          {"job", columns.job},
                          {"message", columns.message},
                          {"phase", columns.phase},
                          {"resource", columns.resource},
                          {"sequence", columns.sequence},
                          {"task", columns.task},
                          {"tick", columns.tick},
                          {"time", columns.time},
                          {"type", columns.type},
                          {"vehicle", columns.vehicle}}},
                        {"event_filters", std::move(filters)},
                        {"panels",
                         {{"architecture", panels.architecture},
                          {"events", panels.events},
                          {"explorer", panels.explorer},
                          {"inspector", panels.inspector},
                          {"resources", panels.resources},
                          {"signals", panels.signals},
                          {"system_builder", panels.system_builder},
                          {"timeline", panels.timeline}}},
                        {"schema_version", workspace.schema_version},
                        {"selected_signals", std::move(signals)},
                        {"splitters",
                         {{"analysis_lower", workspace.analysis_lower_ratio},
                          {"left_sidebar", workspace.left_sidebar_ratio},
                          {"resources_events", workspace.resources_events_ratio},
                          {"right_sidebar", workspace.right_sidebar_ratio}}},
                        {"theme", theme}};
    return document.dump(2) + '\n';
}

ProjectWorkspace parse_project_workspace_json(std::string_view json_text) {
    Json document;
    try {
        document = Json::parse(json_text);
    } catch (const Json::parse_error& error) {
        fail_project("workspace $ (byte " + std::to_string(error.byte) + ')', "malformed JSON");
    }
    require_object(document, "workspace $");
    const std::string workspace_path{"workspace $"};
    const auto& version_value = require_field(document, "schema_version", workspace_path);
    std::uint64_t version = 0;
    if (version_value.is_number_unsigned()) {
        version = version_value.get<std::uint64_t>();
    } else if (version_value.is_number_integer() && version_value.get<std::int64_t>() >= 0) {
        version = static_cast<std::uint64_t>(version_value.get<std::int64_t>());
    } else {
        fail_project("workspace $.schema_version", "must be a nonnegative integer");
    }
    if (version == 1) {
        require_only_fields(document, {"schema_version"}, "workspace $");
        return ProjectWorkspace{};
    }
    if (version != current_project_workspace_schema_version) {
        fail_project("workspace $.schema_version",
                     "unsupported version " + std::to_string(version) + "; expected 1 or " +
                         std::to_string(current_project_workspace_schema_version));
    }
    require_only_fields(document,
                        {"schema_version", "theme", "panels", "splitters", "active_tabs",
                         "event_filters", "event_columns", "selected_signals"},
                        "workspace $");

    ProjectWorkspace workspace;
    const auto read_optional_bool = [](const Json& object, const char* name, bool fallback) {
        const auto found = object.find(name);
        return found != object.end() && found->is_boolean() ? found->get<bool>() : fallback;
    };
    const auto read_optional_float = [](const Json& object, const char* name, float fallback) {
        const auto found = object.find(name);
        return found != object.end() && found->is_number() ? found->get<float>() : fallback;
    };
    if (const auto found = document.find("theme"); found != document.end() && found->is_string()) {
        workspace.theme = found->get<std::string>() == "light" ? GuiTheme::Light : GuiTheme::Dark;
    }
    if (const auto found = document.find("panels"); found != document.end() && found->is_object()) {
        require_only_fields(*found,
                            {"explorer", "system_builder", "inspector", "architecture", "timeline",
                             "signals", "resources", "events"},
                            "workspace $.panels");
        auto& value = workspace.panels;
        value.explorer = read_optional_bool(*found, "explorer", value.explorer);
        value.system_builder = read_optional_bool(*found, "system_builder", value.system_builder);
        value.inspector = read_optional_bool(*found, "inspector", value.inspector);
        value.architecture = read_optional_bool(*found, "architecture", value.architecture);
        value.timeline = read_optional_bool(*found, "timeline", value.timeline);
        value.signals = read_optional_bool(*found, "signals", value.signals);
        value.resources = read_optional_bool(*found, "resources", value.resources);
        value.events = read_optional_bool(*found, "events", value.events);
    }
    if (const auto found = document.find("splitters");
        found != document.end() && found->is_object()) {
        require_only_fields(*found,
                            {"left_sidebar", "right_sidebar", "analysis_lower", "resources_events"},
                            "workspace $.splitters");
        workspace.left_sidebar_ratio =
            read_optional_float(*found, "left_sidebar", workspace.left_sidebar_ratio);
        workspace.right_sidebar_ratio =
            read_optional_float(*found, "right_sidebar", workspace.right_sidebar_ratio);
        workspace.analysis_lower_ratio =
            read_optional_float(*found, "analysis_lower", workspace.analysis_lower_ratio);
        workspace.resources_events_ratio =
            read_optional_float(*found, "resources_events", workspace.resources_events_ratio);
    }
    if (const auto found = document.find("active_tabs");
        found != document.end() && found->is_object()) {
        require_only_fields(*found, {"analysis", "resources"}, "workspace $.active_tabs");
        if (const auto tab = found->find("analysis"); tab != found->end() && tab->is_string()) {
            const auto value = tab->get<std::string>();
            workspace.active_analysis_tab = value == "timeline"  ? GuiAnalysisTab::Timeline
                                            : value == "signals" ? GuiAnalysisTab::Signals
                                                                 : GuiAnalysisTab::Architecture;
        }
        if (const auto tab = found->find("resources"); tab != found->end() && tab->is_string()) {
            workspace.active_resource_tab = tab->get<std::string>() == "utilization"
                                                ? GuiResourceTab::Utilization
                                                : GuiResourceTab::ResourceState;
        }
    }
    if (const auto found = document.find("event_filters");
        found != document.end() && found->is_object()) {
        require_only_fields(*found, {"type", "task", "resource", "vehicle", "text"},
                            "workspace $.event_filters");
        if (const auto text = found->find("text"); text != found->end() && text->is_string()) {
            workspace.event_filters.text = text->get<std::string>();
        }
        const auto read_identifier = [&]<typename Identifier>(const char* name) {
            const auto value = found->find(name);
            if (value != found->end() && value->is_number_unsigned()) {
                return std::optional<Identifier>{Identifier{value->get<std::uint64_t>()}};
            }
            return std::optional<Identifier>{};
        };
        workspace.event_filters.task = read_identifier.template operator()<TaskId>("task");
        workspace.event_filters.resource =
            read_identifier.template operator()<ResourceId>("resource");
        workspace.event_filters.vehicle = read_identifier.template operator()<VehicleId>("vehicle");
        if (const auto type = found->find("type"); type != found->end() && type->is_string()) {
            const auto value = type->get<std::string>();
            static const std::vector<std::pair<std::string, EventType>> types{
                {"job_release", EventType::JobRelease},
                {"job_start", EventType::JobStart},
                {"job_preempt", EventType::JobPreempt},
                {"job_resume", EventType::JobResume},
                {"job_finish", EventType::JobFinish},
                {"deadline_miss", EventType::DeadlineMiss},
                {"message_send", EventType::MessageSend},
                {"message_delivery", EventType::MessageDelivery}};
            const auto match = std::find_if(types.begin(), types.end(),
                                            [&](const auto& row) { return row.first == value; });
            if (match != types.end()) {
                workspace.event_filters.type = match->second;
            }
        }
    }
    if (const auto found = document.find("event_columns");
        found != document.end() && found->is_object()) {
        require_only_fields(*found,
                            {"sequence", "tick", "time", "type", "phase", "task", "job", "resource",
                             "message", "vehicle", "cause"},
                            "workspace $.event_columns");
        auto& value = workspace.event_columns;
        value.sequence = read_optional_bool(*found, "sequence", value.sequence);
        value.tick = read_optional_bool(*found, "tick", value.tick);
        value.time = read_optional_bool(*found, "time", value.time);
        value.type = read_optional_bool(*found, "type", value.type);
        value.phase = read_optional_bool(*found, "phase", value.phase);
        value.task = read_optional_bool(*found, "task", value.task);
        value.job = read_optional_bool(*found, "job", value.job);
        value.resource = read_optional_bool(*found, "resource", value.resource);
        value.message = read_optional_bool(*found, "message", value.message);
        value.vehicle = read_optional_bool(*found, "vehicle", value.vehicle);
        value.cause = read_optional_bool(*found, "cause", value.cause);
    }
    if (const auto found = document.find("selected_signals");
        found != document.end() && found->is_array()) {
        for (const auto& signal : *found) {
            if (!signal.is_object()) {
                continue;
            }
            require_only_fields(signal, {"type", "source_name"}, "workspace $.selected_signals[]");
            const auto type = signal.find("type");
            const auto name = signal.find("source_name");
            if (type == signal.end() || name == signal.end() || !type->is_string() ||
                !name->is_string() || name->get<std::string>().empty()) {
                continue;
            }
            const auto type_name = type->get<std::string>();
            const auto scalar_type = type_name == "integer"   ? GuiSignalScalarType::Integer
                                     : type_name == "boolean" ? GuiSignalScalarType::Boolean
                                                              : GuiSignalScalarType::Real;
            workspace.selected_signals.push_back({scalar_type, name->get<std::string>()});
        }
    }
    normalize_workspace_state(workspace);
    return workspace;
}

std::unique_ptr<ProjectContext>
make_project_context(const std::filesystem::path& root, ProjectMetadata metadata,
                     ExperimentConfig system, RunPlan default_run_plan, ProjectWorkspace workspace,
                     ProjectRuntimeInputs runtime_inputs,
                     std::optional<std::string> workspace_diagnostic) {
    validate_metadata(metadata);
    auto session = std::make_unique<GuiSimulationSession>(
        std::move(system), default_run_plan.stop_tick(), runtime_inputs.functional_model_factory,
        runtime_inputs.signal_registry);
    if (!session->replace_draft(default_run_plan) || !session->apply_draft()) {
        auto message = std::string{"project default run plan could not construct a GUI session"};
        const auto& validation = session->last_validation();
        if (validation.has_value()) {
            const auto& diagnostics = validation.value().diagnostics;
            if (!diagnostics.empty()) {
                message += ": " + diagnostics.back().message;
            }
        }
        throw std::runtime_error{message};
    }
    return std::make_unique<ProjectContext>(
        std::filesystem::weakly_canonical(root), std::move(metadata), std::move(default_run_plan),
        std::move(workspace), std::move(session), std::move(runtime_inputs),
        std::move(workspace_diagnostic));
}

std::unique_ptr<ProjectContext> create_project(const ProjectCreationRequest& request,
                                               ProjectRuntimeInputs runtime_inputs,
                                               const ProjectContentWriter& content_writer) {
    if (request.parent_directory.empty()) {
        fail_project("$.root", "parent directory must not be empty");
    }
    ProjectMetadata metadata{.name = request.name,
                             .scenario_file = request.scenario_file,
                             .scenario_kind = request.scenario_kind};
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

    try {
        std::filesystem::create_directory(root / "run-plans");
        std::filesystem::create_directory(root / "results");

        auto context =
            make_project_context(root, metadata, request.system, request.default_run_plan,
                                 request.workspace, std::move(runtime_inputs));
        write_text_atomically(root / metadata.system_file, system_json);
        write_text_atomically(root / metadata.default_run_plan, run_plan_json);
        write_text_atomically(root / metadata.workspace_file, workspace_json);
        if (content_writer) {
            content_writer(root);
        }
        write_text_atomically(root / "project.json", project_json);
        return context;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        throw;
    }
}

std::unique_ptr<ProjectContext> load_project(const std::filesystem::path& project_file,
                                             const ProjectRuntimeResolver& runtime_resolver) {
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
    if (metadata.scenario_file.has_value()) {
        const auto scenario_path =
            resolve_internal_path(root, *metadata.scenario_file, "$.scenario.file");
        if (!std::filesystem::is_regular_file(scenario_path)) {
            throw std::runtime_error{"project $.scenario.file: required file is missing"};
        }
    }

    auto system =
        load_component("$.system_file", [&] { return load_experiment_config(system_path); });
    auto default_run_plan =
        load_component("$.default_run_plan", [&] { return load_run_plan(run_plan_path, system); });
    ProjectWorkspace workspace;
    std::optional<std::string> workspace_diagnostic;
    try {
        workspace =
            parse_project_workspace_json(read_text_file(workspace_path, "$.workspace_file"));
    } catch (const std::exception& error) {
        workspace = {};
        workspace_diagnostic = std::string{"Workspace defaults used: "} + error.what();
    }

    ProjectRuntimeInputs runtime_inputs;
    if (runtime_resolver) {
        runtime_inputs = runtime_resolver(root, metadata);
    }

    return make_project_context(root, metadata, std::move(system), std::move(default_run_plan),
                                workspace, std::move(runtime_inputs),
                                std::move(workspace_diagnostic));
}

std::unique_ptr<ProjectContext> save_project_as(const ProjectContext& project,
                                                const std::filesystem::path& parent_directory,
                                                std::string new_name,
                                                const ProjectRuntimeResolver& runtime_resolver) {
    validate_project_name(new_name);
    if (parent_directory.empty()) {
        fail_project("$.root", "parent directory must not be empty");
    }
    const auto parent = std::filesystem::absolute(parent_directory).lexically_normal();
    std::filesystem::create_directories(parent);
    const auto target = parent / new_name;
    if (std::filesystem::exists(target)) {
        throw std::runtime_error{"project directory already exists: " + target.string()};
    }
    try {
        std::filesystem::create_directory(target);
        for (const auto& entry : std::filesystem::directory_iterator(project.root())) {
            if (entry.path().filename() == "project.json") {
                continue;
            }
            std::filesystem::copy(entry.path(), target / entry.path().filename(),
                                  std::filesystem::copy_options::recursive);
        }
        auto metadata = project.metadata();
        metadata.name = std::move(new_name);
        write_text_atomically(target / metadata.system_file,
                              serialize_experiment_config_json(project.session().config()));
        write_text_atomically(
            target / metadata.default_run_plan,
            serialize_run_plan_json(project.session().config(), accepted_project_plan(project)));
        write_text_atomically(target / metadata.workspace_file,
                              serialize_project_workspace_json(project.workspace()));
        write_text_atomically(target / "project.json", serialize_project_metadata_json(metadata));
        return load_project(target / "project.json", runtime_resolver);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(target, ignored);
        throw;
    }
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
        serialize_run_plan_json(project.session().config(), accepted_project_plan(project));
    const auto workspace_json = serialize_project_workspace_json(project.workspace());
    const auto project_json = serialize_project_metadata_json(metadata);

    write_text_atomically(system_path, system_json);
    write_text_atomically(run_plan_path, run_plan_json);
    write_text_atomically(workspace_path, workspace_json);
    write_text_atomically(project.root() / "project.json", project_json);
}

} // namespace cpssim
