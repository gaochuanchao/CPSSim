/*** Build Bosch specifications, FMI runtime input, and a paused project session. ***/

#include "cpssim/application/bosch_project_factory.hpp"

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/bosch/example_data.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace cpssim {
namespace {

using Json = nlohmann::json;
constexpr std::uint32_t bosch_settings_schema_version = 1;

struct BoschProjectSettings {
    std::filesystem::path trajectory_directory{"trajectory"};
    BoschReferenceScenario scenario{BoschReferenceScenario::Dedicated};
};

Fmi2ModelInfo model_info(const std::filesystem::path& shared_library, std::string instance_name) {
    return {.shared_library = shared_library,
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = std::move(instance_name)};
}

std::vector<GuiSignalDescriptor> bosch_signal_registry() {
    return {
        {{GuiSignalScalarType::Real, "lateral_error"},
         "Bosch/Control/Lateral error",
         "Lateral error",
         "m",
         "Bosch FMU"},
        {{GuiSignalScalarType::Real, "actuator_command"},
         "Bosch/Control/Actuator command",
         "Actuator command",
         "",
         "Bosch FMU"},
        {{GuiSignalScalarType::Real, "rolling_real"},
         "Bosch/Timing/Rolling real",
         "Rolling real",
         "",
         "Bosch FMU"},
        {{GuiSignalScalarType::Real, "rolling_remote"},
         "Bosch/Timing/Rolling remote",
         "Rolling remote",
         "",
         "Bosch FMU"},
        {{GuiSignalScalarType::Integer, "violation_counter"},
         "Bosch/Timing/Violations",
         "Violation counter",
         "",
         "Bosch FMU"},
        {{GuiSignalScalarType::Boolean, "critical_section"},
         "Bosch/Timing/Critical section",
         "Critical section",
         "",
         "Bosch FMU"},
    };
}

ProjectRuntimeInputs make_runtime(const std::filesystem::path& shared_library,
                                  std::vector<BoschTrajectorySample> trajectory,
                                  std::string instance_name) {
    auto samples =
        std::make_shared<const std::vector<BoschTrajectorySample>>(std::move(trajectory));
    auto info = model_info(shared_library, std::move(instance_name));
    return {.functional_model_factory =
                [info = std::move(info), samples] {
                    return std::make_unique<BoschFmi2FunctionalModel>(info, *samples);
                },
            .signal_registry = bosch_signal_registry()};
}

std::string serialize_settings(const BoschProjectSettings& settings) {
    const auto normalized = settings.trajectory_directory.lexically_normal();
    if (settings.trajectory_directory.empty() || settings.trajectory_directory.is_absolute() ||
        normalized.empty() || normalized == "." || *normalized.begin() == ".." ||
        settings.trajectory_directory.generic_string().find('\\') != std::string::npos) {
        throw std::invalid_argument{"Bosch trajectory path must stay inside the project"};
    }
    return Json{{"schema_version", bosch_settings_schema_version},
                {"trajectory_directory", settings.trajectory_directory.generic_string()},
                {"scenario", bosch_reference_scenario_name(settings.scenario)}}
               .dump(2) +
           '\n';
}

BoschProjectSettings load_settings(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot read Bosch project settings: " + path.string()};
    }
    Json document;
    try {
        input >> document;
    } catch (const Json::exception&) {
        throw std::invalid_argument{"Bosch project settings contain malformed JSON"};
    }
    if (!document.is_object() || document.size() != 3 || !document.contains("schema_version") ||
        !document.contains("trajectory_directory") || !document.contains("scenario")) {
        throw std::invalid_argument{"Bosch project settings do not match schema version 1"};
    }
    if (!document["schema_version"].is_number_unsigned() ||
        document["schema_version"].get<std::uint32_t>() != bosch_settings_schema_version ||
        !document["trajectory_directory"].is_string() || !document["scenario"].is_string()) {
        throw std::invalid_argument{"Bosch project settings contain invalid field types"};
    }
    BoschProjectSettings settings{
        .trajectory_directory = document["trajectory_directory"].get<std::string>(),
        .scenario = parse_bosch_reference_scenario(document["scenario"].get<std::string>())};
    static_cast<void>(serialize_settings(settings));
    return settings;
}

void write_settings(const std::filesystem::path& path, const BoschProjectSettings& settings) {
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output{temporary, std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"cannot create Bosch project settings"};
    }
    output << serialize_settings(settings);
    output.close();
    if (!output) {
        throw std::runtime_error{"cannot write Bosch project settings"};
    }
    std::filesystem::rename(temporary, path);
}

} // namespace

void validate_bosch_project_request(const BoschProjectRequest& request) {
    if (request.parent_directory.empty()) {
        throw std::invalid_argument{"project parent directory must not be empty"};
    }
    if (request.name.empty() || std::filesystem::path{request.name}.has_parent_path() ||
        request.name == "." || request.name == ".." ||
        request.name.find('/') != std::string::npos ||
        request.name.find('\\') != std::string::npos) {
        throw std::invalid_argument{"project name must be a single directory name"};
    }
    if (request.trajectory_directory.empty() ||
        !std::filesystem::is_directory(request.trajectory_directory)) {
        throw std::invalid_argument{"Bosch trajectory directory must exist"};
    }
    if (request.reference_root.empty() || !std::filesystem::is_directory(request.reference_root)) {
        throw std::invalid_argument{"Bosch reference directory must exist"};
    }
    if (request.shared_library.empty() ||
        !std::filesystem::is_regular_file(request.shared_library)) {
        throw std::invalid_argument{"Bosch FMU shared library must exist"};
    }
    if (request.stop_tick.has_value() && *request.stop_tick < 0) {
        throw std::invalid_argument{"stop tick must be nonnegative"};
    }
}

std::filesystem::path resolve_bundled_bosch_fmu(const std::filesystem::path& executable_path) {
#if defined(_WIN32)
    constexpr auto library_name = "LateralMotionControl.dll";
#elif defined(__APPLE__)
    constexpr auto library_name = "LateralMotionControl.dylib";
#else
    constexpr auto library_name = "LateralMotionControl.so";
#endif
    return std::filesystem::absolute(executable_path).parent_path() / library_name;
}

std::unique_ptr<ProjectContext> create_bosch_project(const BoschProjectRequest& request) {
    validate_bosch_project_request(request);
    auto trajectory = load_bosch_example_trajectory(request.trajectory_directory);
    const auto final_tick = static_cast<Tick>(trajectory.size() - 1);
    const auto stop_tick = request.stop_tick.value_or(final_tick);
    if (stop_tick > final_tick) {
        throw std::invalid_argument{"stop tick exceeds the final supplied example tick"};
    }
    auto inputs = load_bosch_reference_inputs(request.reference_root, request.scenario);
    auto plan = build_run_plan(
        inputs.config, RunPlanRequest{.stop_tick = stop_tick, .assignments = inputs.assignments});
    if (!plan.plan.has_value() || !plan.diagnostics.empty()) {
        throw std::logic_error{"Bosch reference assignments did not form a valid run plan"};
    }
    auto runtime =
        make_runtime(request.shared_library, std::move(trajectory), "cpssim_gui_" + request.name);
    ProjectCreationRequest creation{.parent_directory = request.parent_directory,
                                    .name = request.name,
                                    .system = std::move(inputs.config),
                                    .default_run_plan = std::move(plan.plan.value()),
                                    .scenario_file = "bosch.json",
                                    .scenario_kind = "bosch"};
    const auto source = std::filesystem::weakly_canonical(request.trajectory_directory);
    return create_project(creation, std::move(runtime),
                          [source, scenario = request.scenario](const auto& root) {
                              std::filesystem::copy(source, root / "trajectory",
                                                    std::filesystem::copy_options::recursive);
                              write_settings(root / "bosch.json", {.scenario = scenario});
                          });
}

ProjectRuntimeInputs resolve_bosch_project_runtime(
    const std::filesystem::path& project_root, const ProjectMetadata& metadata,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const std::filesystem::path& reference_root, const std::filesystem::path& shared_library) {
    if (metadata.scenario_kind == "generic") {
        return {};
    }
    if (metadata.scenario_kind != "bosch" || !metadata.scenario_file.has_value()) {
        throw std::invalid_argument{"unsupported project scenario kind: " + metadata.scenario_kind};
    }
    const auto settings_path =
        std::filesystem::weakly_canonical(project_root / *metadata.scenario_file);
    const auto settings = load_settings(settings_path);
    const auto trajectory_path =
        std::filesystem::weakly_canonical(project_root / settings.trajectory_directory);
    const auto canonical_root = std::filesystem::weakly_canonical(project_root);
    const auto relative = trajectory_path.lexically_relative(canonical_root);
    if (relative.empty() || *relative.begin() == ".." ||
        !std::filesystem::is_directory(trajectory_path)) {
        throw std::invalid_argument{"Bosch trajectory directory escapes or is missing"};
    }
    static_cast<void>(load_bosch_reference_inputs(reference_root, settings.scenario));
    return make_runtime(shared_library, load_bosch_example_trajectory(trajectory_path),
                        "cpssim_gui_" + metadata.name);
}

} // namespace cpssim
