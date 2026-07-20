/*** Implement deterministic raw result schemas and atomic directory publication. ***/

#include "cpssim/application/result_export.hpp"

#include "cpssim/config/json_config.hpp"
#include "cpssim/config/json_run_plan.hpp"
#include "cpssim/core/version.hpp"
#include "cpssim/trace/event_json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace cpssim {
namespace {

using Json = nlohmann::ordered_json;

std::string csv_cell(std::string_view value) {
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) {
        return std::string{value};
    }
    std::string result{"\""};
    for (const auto character : value) {
        if (character == '"') {
            result += "\"\"";
        } else {
            result += character;
        }
    }
    return result + '"';
}

template <typename Id> std::string optional_id(const std::optional<Id>& id) {
    return id.has_value() ? std::to_string(id->value()) : std::string{};
}

std::string optional_sequence(const std::optional<EventSequence>& sequence) {
    return sequence.has_value() ? std::to_string(sequence->value()) : std::string{};
}

std::string physical_seconds(Tick tick, PhysicalDuration period) {
    const auto seconds = static_cast<long double>(tick) * static_cast<long double>(period.count()) /
                         1'000'000'000.0L;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<long double>::max_digits10) << seconds;
    return output.str();
}

bool selected(Tick tick, const std::optional<GuiTickRange>& range) {
    return !range.has_value() || range->contains(tick);
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"cannot create result file '" + path.string() + "'"};
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error{"cannot write result file '" + path.string() + "'"};
    }
}

std::string checksum(std::string_view contents) {
    // FNV-1a is intentionally labeled: this reproducibility checksum is not a security primitive.
    std::uint64_t value = 14695981039346656037ULL;
    for (const auto character : contents) {
        value ^= static_cast<std::uint8_t>(character);
        value *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

void validate_run_id(const std::string& run_id) {
    const std::filesystem::path path{run_id};
    if (run_id.empty() || path.is_absolute() || path.has_parent_path() || run_id == "." ||
        run_id == ".." || run_id.find('/') != std::string::npos ||
        run_id.find('\\') != std::string::npos) {
        throw std::invalid_argument{"run ID must be a single directory name"};
    }
}

std::filesystem::path unique_temporary_directory(const std::filesystem::path& destination,
                                                 const std::string& run_id) {
    for (std::uint32_t suffix = 0; suffix < 10'000; ++suffix) {
        const auto candidate = destination / ("." + run_id + ".tmp-" + std::to_string(suffix));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) {
            return candidate;
        }
        if (error && error != std::errc::file_exists) {
            throw std::runtime_error{"cannot create temporary result directory: " +
                                     error.message()};
        }
    }
    throw std::runtime_error{"cannot allocate a temporary result directory"};
}

double metric_seconds(double ticks, PhysicalDuration tick_period) {
    return ticks * static_cast<double>(tick_period.count()) / 1.0e9;
}

Json tick_statistics_json(const std::optional<TickStatistics>& statistics,
                          PhysicalDuration tick_period) {
    if (!statistics.has_value()) {
        return nullptr;
    }
    return {
        {"count", statistics->count},
        {"maximum_seconds", metric_seconds(static_cast<double>(statistics->maximum), tick_period)},
        {"maximum_ticks", statistics->maximum},
        {"mean_seconds", metric_seconds(statistics->mean(), tick_period)},
        {"mean_ticks", statistics->mean()},
        {"minimum_seconds", metric_seconds(static_cast<double>(statistics->minimum), tick_period)},
        {"minimum_ticks", statistics->minimum},
        {"total_ticks", statistics->total}};
}

std::optional<GuiTickRange> export_range(const RunExportOptions& options) {
    if (options.scope == RunExportScope::Complete) {
        return std::nullopt;
    }
    if (!options.selected_range.has_value() || options.selected_range->begin_tick < 0 ||
        options.selected_range->end_tick < options.selected_range->begin_tick) {
        throw std::invalid_argument{"selected export range is missing or invalid"};
    }
    return options.selected_range;
}

std::uint64_t count_events(const SimulationSnapshot& snapshot,
                           const std::optional<GuiTickRange>& range) {
    return static_cast<std::uint64_t>(
        std::count_if(snapshot.event_log.begin(), snapshot.event_log.end(),
                      [&range](const Event& event) { return selected(event.tick(), range); }));
}

std::uint64_t count_signals(const RunResult& result, const std::optional<GuiTickRange>& range) {
    if (!result.signals.model.has_value() || !result.signals.diagnostics.empty()) {
        throw std::invalid_argument{"functional signals are invalid and cannot be exported"};
    }
    const auto& model = result.signals.model.value();
    std::uint64_t count = 0;
    for (const auto& series : model.series) {
        count += static_cast<std::uint64_t>(std::count_if(
            series.samples.begin(), series.samples.end(),
            [&range](const GuiScalarSample& sample) { return selected(sample.tick, range); }));
    }
    return count;
}

std::string serialize_events_jsonl(const SimulationSnapshot& snapshot,
                                   const std::optional<GuiTickRange>& range) {
    std::string result;
    for (const auto& event : snapshot.event_log) {
        if (selected(event.tick(), range)) {
            result += serialize_event_json_line(event);
        }
    }
    return result;
}

} // namespace

std::string serialize_run_manifest_json(const RunManifest& manifest) {
    Json scenario{{"kind", manifest.scenario_kind}};
    if (manifest.scenario.bosch_trajectory.has_value()) {
        scenario["bosch_trajectory"] = *manifest.scenario.bosch_trajectory;
    }
    if (manifest.scenario.fmu_identity.has_value()) {
        scenario["fmu_identity"] = *manifest.scenario.fmu_identity;
    }
    if (manifest.scenario.fmu_path.has_value()) {
        scenario["fmu_path"] = manifest.scenario.fmu_path->generic_string();
    }
    return Json{
               {"created_at_utc", manifest.created_at_utc},
               {"cpssim_version", manifest.cpssim_version},
               {"policy", manifest.policy},
               {"project_name", manifest.project_name},
               {"run_id", manifest.run_id},
               {"run_plan",
                {{"checksum", manifest.run_plan_checksum}, {"file", manifest.run_plan_file}}},
               {"scenario", std::move(scenario)},
               {"schema_version", manifest.schema_version},
               {"stop_tick", manifest.stop_tick},
               {"system", {{"checksum", manifest.system_checksum}, {"file", manifest.system_file}}}}
               .dump(2) +
           '\n';
}

RunManifest parse_run_manifest_json(std::string_view json_text) {
    try {
        const auto document = Json::parse(json_text);
        if (!document.is_object() || document.at("schema_version").get<std::uint32_t>() !=
                                         current_run_manifest_schema_version) {
            throw std::invalid_argument{"unsupported run manifest schema"};
        }
        const auto& scenario = document.at("scenario");
        RunManifest manifest{
            .schema_version = document.at("schema_version").get<std::uint32_t>(),
            .cpssim_version = document.at("cpssim_version").get<std::string>(),
            .project_name = document.at("project_name").get<std::string>(),
            .run_id = document.at("run_id").get<std::string>(),
            .created_at_utc = document.at("created_at_utc").get<std::string>(),
            .system_file = document.at("system").at("file").get<std::string>(),
            .run_plan_file = document.at("run_plan").at("file").get<std::string>(),
            .system_checksum = document.at("system").at("checksum").get<std::string>(),
            .run_plan_checksum = document.at("run_plan").at("checksum").get<std::string>(),
            .policy = document.at("policy").get<std::string>(),
            .stop_tick = document.at("stop_tick").get<Tick>(),
            .scenario_kind = scenario.at("kind").get<std::string>(),
            .scenario = {}};
        if (scenario.contains("bosch_trajectory")) {
            manifest.scenario.bosch_trajectory = scenario.at("bosch_trajectory").get<std::string>();
        }
        if (scenario.contains("fmu_identity")) {
            manifest.scenario.fmu_identity = scenario.at("fmu_identity").get<std::string>();
        }
        if (scenario.contains("fmu_path")) {
            manifest.scenario.fmu_path = scenario.at("fmu_path").get<std::string>();
        }
        validate_run_id(manifest.run_id);
        return manifest;
    } catch (const Json::exception&) {
        throw std::invalid_argument{"run manifest contains malformed or invalid JSON"};
    }
}

std::string serialize_run_metrics_json(const RunMetrics& metrics) {
    Json tasks = Json::array();
    for (const auto& task : metrics.task_responses) {
        tasks.push_back(
            {{"response_time", tick_statistics_json(task.response_time, metrics.tick_period)},
             {"task_id", task.task_id.value()},
             {"task_name", task.task_name}});
    }
    Json resources = Json::array();
    for (const auto& resource : metrics.resources) {
        resources.push_back(
            {{"busy_ticks", resource.busy_ticks},
             {"idle_ticks", resource.idle_ticks},
             {"resource_id", resource.resource_id.value()},
             {"resource_name", resource.resource_name},
             {"utilization",
              resource.utilization.has_value() ? Json{*resource.utilization} : Json{nullptr}}});
    }
    return Json{{"completed_jobs", metrics.completed_jobs},
                {"deadline_misses", metrics.deadline_misses},
                {"event_count", metrics.event_count},
                {"horizon_tick", metrics.horizon_tick},
                {"horizon_time_ns", metrics.horizon_time.has_value()
                                        ? Json{metrics.horizon_time->count()}
                                        : Json{nullptr}},
                {"messages",
                 {{"delivered", metrics.messages.delivered},
                  {"delivery_delay",
                   tick_statistics_json(metrics.messages.delivery_delay, metrics.tick_period)},
                  {"sent", metrics.messages.sent}}},
                {"preemptions", metrics.preemptions},
                {"resources", std::move(resources)},
                {"schema_version", 1},
                {"tick_period_ns", metrics.tick_period.count()},
                {"task_response_times", std::move(tasks)}}
               .dump(2) +
           '\n';
}

std::string serialize_run_metrics_csv(const RunMetrics& metrics) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "category,id,name,metric,count,minimum_ticks,maximum_ticks,total_ticks,mean_ticks,"
              "value\n";
    output << "run,,,event_count,,,,,," << metrics.event_count << '\n';
    output << "run,,,horizon_tick,,,,,," << metrics.horizon_tick << '\n';
    output << "run,,,tick_period_ns,,,,,," << metrics.tick_period.count() << '\n';
    output << "run,,,completed_jobs,,,,,," << metrics.completed_jobs << '\n';
    output << "run,,,deadline_misses,,,,,," << metrics.deadline_misses << '\n';
    output << "run,,,preemptions,,,,,," << metrics.preemptions << '\n';
    for (const auto& task : metrics.task_responses) {
        output << "task," << task.task_id.value() << ',' << csv_cell(task.task_name)
               << ",response_time,";
        if (task.response_time.has_value()) {
            const auto& value = *task.response_time;
            output << value.count << ',' << value.minimum << ',' << value.maximum << ','
                   << value.total << ',' << std::setprecision(17) << value.mean();
        } else {
            output << ",,,,";
        }
        output << ",\n";
    }
    for (const auto& resource : metrics.resources) {
        output << "resource," << resource.resource_id.value() << ','
               << csv_cell(resource.resource_name) << ",busy_ticks,,,,,," << resource.busy_ticks
               << '\n';
        output << "resource," << resource.resource_id.value() << ','
               << csv_cell(resource.resource_name) << ",idle_ticks,,,,,," << resource.idle_ticks
               << '\n';
        output << "resource," << resource.resource_id.value() << ','
               << csv_cell(resource.resource_name) << ",utilization,,,,,,";
        if (resource.utilization.has_value()) {
            output << std::setprecision(17) << *resource.utilization;
        }
        output << '\n';
    }
    output << "message,,,sent,,,,,," << metrics.messages.sent << '\n';
    output << "message,,,delivered,,,,,," << metrics.messages.delivered << '\n';
    output << "message,,,delivery_delay,";
    if (metrics.messages.delivery_delay.has_value()) {
        const auto& value = *metrics.messages.delivery_delay;
        output << value.count << ',' << value.minimum << ',' << value.maximum << ',' << value.total
               << ',' << std::setprecision(17) << value.mean();
    } else {
        output << ",,,,";
    }
    output << ",\n";
    return output.str();
}

std::string serialize_events_csv(const SimulationSnapshot& snapshot,
                                 std::optional<GuiTickRange> range) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "sequence,tick,time_seconds,type,phase,task_id,job_id,resource_id,message_id,vehicle_"
              "id,cause_sequence\n";
    for (const auto& event : snapshot.event_log) {
        if (!selected(event.tick(), range)) {
            continue;
        }
        const auto& entities = event.entities();
        output << event.sequence().value() << ',' << event.tick() << ','
               << physical_seconds(event.tick(), snapshot.experiment.tick_period) << ','
               << canonical_event_type_name(event.type()) << ','
               << canonical_event_phase_name(event.phase()) << ',' << optional_id(entities.task_id)
               << ',' << optional_id(entities.job_id) << ',' << optional_id(entities.resource_id)
               << ',' << optional_id(entities.message_id) << ',' << optional_id(entities.vehicle_id)
               << ',' << optional_sequence(event.cause_sequence()) << '\n';
    }
    return output.str();
}

std::string serialize_signals_csv(const RunResult& result, std::optional<GuiTickRange> range) {
    if (!result.signals.model.has_value() || !result.signals.diagnostics.empty()) {
        throw std::invalid_argument{"functional signals are invalid and cannot be exported"};
    }
    const auto& model = result.signals.model.value();
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "tick,time_seconds,type,source_name,path,display_name,unit,value\n";
    for (const auto& series : model.series) {
        const auto type = series.descriptor.id.scalar_type == GuiSignalScalarType::Real ? "real"
                          : series.descriptor.id.scalar_type == GuiSignalScalarType::Integer
                              ? "integer"
                              : "boolean";
        for (const auto& sample : series.samples) {
            if (!selected(sample.tick, range)) {
                continue;
            }
            output << sample.tick << ','
                   << physical_seconds(sample.tick, result.snapshot.experiment.tick_period) << ','
                   << type << ',' << csv_cell(series.descriptor.id.source_name) << ','
                   << csv_cell(series.descriptor.path) << ','
                   << csv_cell(series.descriptor.display_name) << ','
                   << csv_cell(series.descriptor.unit) << ',';
            std::visit(
                [&output](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, bool>) {
                        output << (value ? "true" : "false");
                    } else if constexpr (std::is_same_v<Value, double>) {
                        output << std::setprecision(17) << value;
                    } else {
                        output << value;
                    }
                },
                sample.value);
            output << '\n';
        }
    }
    return output.str();
}

RunExportArtifacts export_run_result(const ProjectContext& project, const RunResult& result,
                                     const RunExportOptions& options) {
    validate_run_id(options.run_id);
    const auto range = export_range(options);
    auto destination = options.destination_directory.empty() ? project.root() / "results"
                                                             : options.destination_directory;
    std::filesystem::create_directories(destination);
    const auto target = destination / options.run_id;
    if (std::filesystem::exists(target)) {
        throw std::invalid_argument{"run result already exists: " + target.string()};
    }
    const auto temporary = unique_temporary_directory(destination, options.run_id);
    try {
        const auto system_json = serialize_experiment_config_json(project.session().config());
        const auto* active_plan = project.session().active_plan();
        const auto& plan = active_plan != nullptr ? *active_plan : project.default_run_plan();
        const auto plan_json = serialize_run_plan_json(project.session().config(), plan);
        const RunManifest manifest{
            .schema_version = current_run_manifest_schema_version,
            .cpssim_version = std::string{version()},
            .project_name = project.metadata().name,
            .run_id = options.run_id,
            .created_at_utc = options.created_at_utc.empty() ? utc_now() : options.created_at_utc,
            .system_file = "system.json",
            .run_plan_file = "run-plan.json",
            .system_checksum = checksum(system_json),
            .run_plan_checksum = checksum(plan_json),
            .policy = "fixed_priority",
            .stop_tick = plan.stop_tick(),
            .scenario_kind = project.metadata().scenario_kind,
            .scenario = options.scenario};
        const auto exported_metrics =
            range.has_value() ? derive_run_metrics(select_run_result_range(
                                    result.snapshot, range->begin_tick, range->end_tick))
                              : result.metrics;

        write_file(temporary / "system.json", system_json);
        write_file(temporary / "run-plan.json", plan_json);
        write_file(temporary / "events.jsonl", serialize_events_jsonl(result.snapshot, range));
        write_file(temporary / "events.csv", serialize_events_csv(result.snapshot, range));
        write_file(temporary / "signals.csv", serialize_signals_csv(result, range));
        write_file(temporary / "metrics.json", serialize_run_metrics_json(exported_metrics));
        write_file(temporary / "metrics.csv", serialize_run_metrics_csv(exported_metrics));
        if (options.include_excel) {
            auto workbook_result = result;
            workbook_result.metrics = exported_metrics;
            write_results_workbook(temporary / "results.xlsx", project, workbook_result, range,
                                   options.control_metrics);
        }
        write_file(temporary / "manifest.json", serialize_run_manifest_json(manifest));
        std::filesystem::rename(temporary, target);
        return {.run_directory = target,
                .manifest = manifest,
                .event_rows = count_events(result.snapshot, range),
                .signal_rows = count_signals(result, range)};
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary, ignored);
        throw;
    }
}

} // namespace cpssim
