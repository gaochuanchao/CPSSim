/***
 * File: src/cpssim/conformance/bosch_reference.cpp
 * Purpose: Build the two Bosch timing scenarios from their pinned task CSVs,
 *          run CPSSim, normalize three observable streams, and find the first
 *          divergence.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: MATLAB row IDs, floating-point seconds, and packet IDs are outside
 *        the comparison. Bosch trigger tick, column, and name are exact.
 ***/

#include "cpssim/conformance/bosch_reference.hpp"

#include "cpssim/bosch/trigger_encoder.hpp"
#include "cpssim/kernel/simulation_engine.hpp"
#include "cpssim/model/experiment_config.hpp"
#include "cpssim/policy/fixed_priority.hpp"
#include "cpssim/policy/resource_allocator.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cpssim {

namespace {

constexpr Tick reference_stop_tick = 150000;
constexpr Tick reference_send_offset = 1;
constexpr Tick reference_network_delay = 80;
constexpr auto reference_tick_period = std::chrono::microseconds{100};

struct NamedTask {
    TaskId id;
    std::string name;
};

struct NamedResource {
    ResourceId id;
    std::string name;
};

struct ReferenceExperiment {
    ExperimentConfig config;
    std::vector<TaskAssignment> assignments;
    std::vector<NamedTask> tasks;
    std::vector<NamedResource> resources;
};

struct SchedulerObservation {
    Tick tick;
    std::string type;
    TaskId task_id;
    std::string task_name;
    JobId job_id;
    std::string resource_name;
};

struct NetworkObservation {
    TaskId source_task_id;
    std::string source_task_name;
    JobId source_job_id;
    TaskId destination_task_id;
    std::string destination_task_name;
    Tick publish_tick;
    Tick send_tick;
    Tick delivery_tick;
    bool send_in_trace;
    bool delivery_in_trace;
};

struct TriggerObservation {
    Tick tick;
    std::size_t column;
    std::string name;
};

/*** Removes the Windows carriage return retained by text-mode CSV input. ***/
void remove_trailing_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

/***
 * Splits the pinned simple CSV schema. Quoted fields are rejected because the
 * reference generator emits only unquoted numeric identifiers and names.
 ***/
std::vector<std::string> split_csv_row(const std::string& line, const std::filesystem::path& path,
                                       std::size_t row_number) {
    if (line.find('"') != std::string::npos) {
        throw std::runtime_error{path.string() + ": quoted CSV is outside the pinned schema"};
    }

    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (true) {
        const auto comma = line.find(',', begin);
        if (comma == std::string::npos) {
            fields.push_back(line.substr(begin));
            break;
        }
        fields.push_back(line.substr(begin, comma - begin));
        begin = comma + 1;
    }
    if (fields.empty()) {
        throw std::runtime_error{path.string() + ": empty CSV row " + std::to_string(row_number)};
    }
    return fields;
}

/*** Opens a reference CSV or reports its complete path. ***/
std::ifstream open_csv(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot open reference CSV: " + path.string()};
    }
    return input;
}

/*** Requires the exact pinned header before any data row is interpreted. ***/
void require_header(std::ifstream& input, const std::filesystem::path& path,
                    std::string_view expected_header) {
    std::string header;
    if (!std::getline(input, header)) {
        throw std::runtime_error{"reference CSV has no header: " + path.string()};
    }
    remove_trailing_carriage_return(header);
    if (header != expected_header) {
        throw std::runtime_error{"reference CSV header differs from the pinned schema: " +
                                 path.string()};
    }
}

/*** Parses one unsigned integer field without accepting signs or suffixes. ***/
std::uint64_t parse_unsigned(std::string_view text, const std::filesystem::path& path,
                             std::size_t row_number, std::string_view field_name) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::runtime_error{path.string() + ": invalid " + std::string{field_name} +
                                 " at row " + std::to_string(row_number)};
    }
    return value;
}

/*** Parses a nonnegative canonical Tick and rejects overflow. ***/
Tick parse_tick(std::string_view text, const std::filesystem::path& path, std::size_t row_number,
                std::string_view field_name) {
    const auto value = parse_unsigned(text, path, row_number, field_name);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<Tick>::max())) {
        throw std::runtime_error{path.string() + ": " + std::string{field_name} +
                                 " exceeds Tick at row " + std::to_string(row_number)};
    }
    return static_cast<Tick>(value);
}

/*** Parses the reference's explicit zero-or-one horizon marker. ***/
bool parse_boolean(std::string_view text, const std::filesystem::path& path, std::size_t row_number,
                   std::string_view field_name) {
    if (text == "0") {
        return false;
    }
    if (text == "1") {
        return true;
    }
    throw std::runtime_error{path.string() + ": invalid " + std::string{field_name} + " at row " +
                             std::to_string(row_number)};
}

/*** Finds a stable task ID by the reference task name. ***/
TaskId task_id_by_name(const std::vector<NamedTask>& tasks, std::string_view name) {
    for (const auto& task : tasks) {
        if (task.name == name) {
            return task.id;
        }
    }
    throw std::runtime_error{"reference names an unknown task: " + std::string{name}};
}

/*** Finds the configured task name used for a normalized observation. ***/
const std::string& task_name_by_id(const std::vector<NamedTask>& tasks, TaskId id) {
    for (const auto& task : tasks) {
        if (task.id == id) {
            return task.name;
        }
    }
    throw std::logic_error{"CPSSim event references an unknown task"};
}

/*** Finds the configured resource name used by the MATLAB table. ***/
const std::string& resource_name_by_id(const std::vector<NamedResource>& resources, ResourceId id) {
    for (const auto& resource : resources) {
        if (resource.id == id) {
            return resource.name;
        }
    }
    throw std::logic_error{"CPSSim event references an unknown resource"};
}

/*** Reuses the first-seen reference resource or creates its stable runtime ID. ***/
ResourceId find_or_add_resource(std::vector<ResourceSpec>& specs, std::vector<NamedResource>& names,
                                const std::string& resource_name) {
    for (const auto& resource : names) {
        if (resource.name == resource_name) {
            return resource.id;
        }
    }
    const ResourceId id{static_cast<std::uint64_t>(names.size() + 1)};
    specs.emplace_back(id, resource_name);
    names.push_back({.id = id, .name = resource_name});
    return id;
}

/***
 * Reads tasks.csv into generic task/resource specifications. Resource IDs use
 * first appearance, reproducing the reference's deterministic start order.
 ***/
ReferenceExperiment load_reference_experiment(const std::filesystem::path& scenario_path) {
    const auto path = scenario_path / "tasks.csv";
    auto input = open_csv(path);
    require_header(input, path,
                   "taskId,name,platform,resource,periodTicks,executionTicks,deadlineTicks,"
                   "offsetTicks,priority,activationColumn,finishColumn");

    std::vector<ResourceSpec> resource_specs;
    std::vector<TaskSpec> task_specs;
    std::vector<TaskResourceProfile> profiles;
    std::vector<TaskAssignment> assignments;
    std::vector<NamedTask> task_names;
    std::vector<NamedResource> resource_names;

    std::string line;
    std::size_t row_number = 1;
    while (std::getline(input, line)) {
        ++row_number;
        remove_trailing_carriage_return(line);
        const auto fields = split_csv_row(line, path, row_number);
        if (fields.size() != 11) {
            throw std::runtime_error{path.string() + ": expected 11 fields at row " +
                                     std::to_string(row_number)};
        }

        const TaskId task_id{parse_unsigned(fields[0], path, row_number, "taskId")};
        const auto resource_id = find_or_add_resource(resource_specs, resource_names, fields[3]);
        const auto priority_value = parse_unsigned(fields[8], path, row_number, "priority");
        if (priority_value > static_cast<std::uint64_t>(std::numeric_limits<Priority>::max())) {
            throw std::runtime_error{path.string() + ": priority is too large at row " +
                                     std::to_string(row_number)};
        }

        task_specs.emplace_back(
            task_id, fields[1],
            PeriodicTimingSpec{.period = parse_tick(fields[4], path, row_number, "periodTicks"),
                               .deadline = parse_tick(fields[6], path, row_number, "deadlineTicks"),
                               .offset = parse_tick(fields[7], path, row_number, "offsetTicks")},
            static_cast<Priority>(priority_value));
        profiles.push_back(
            {.task_id = task_id,
             .resource_id = resource_id,
             .execution_time = parse_tick(fields[5], path, row_number, "executionTicks")});
        assignments.push_back({.task_id = task_id, .resource_id = resource_id});
        task_names.push_back({.id = task_id, .name = fields[1]});
    }
    if (task_specs.empty()) {
        throw std::runtime_error{"reference task table is empty: " + path.string()};
    }

    const std::vector<MessageRouteSpec> routes{
        {.source_task_id = task_id_by_name(task_names, "Sensor"),
         .destination_task_id = task_id_by_name(task_names, "Estimator"),
         .send_offset = reference_send_offset,
         .delay = reference_network_delay},
        {.source_task_id = task_id_by_name(task_names, "Merger"),
         .destination_task_id = task_id_by_name(task_names, "Actuator"),
         .send_offset = reference_send_offset,
         .delay = reference_network_delay},
    };

    ExperimentConfig config{
        reference_tick_period,     SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        std::move(resource_specs), std::move(task_specs),
        std::move(profiles),       routes};
    return {.config = std::move(config),
            .assignments = std::move(assignments),
            .tasks = std::move(task_names),
            .resources = std::move(resource_names)};
}

/*** Converts generic event categories to the MATLAB scheduler vocabulary. ***/
std::optional<std::string_view> scheduler_event_name(EventType type) {
    switch (type) {
    case EventType::JobRelease:
        return "release";
    case EventType::JobStart:
        return "start";
    case EventType::JobPreempt:
        return "preempt";
    case EventType::JobResume:
        return "resume";
    case EventType::JobFinish:
        return "finish";
    case EventType::DeadlineMiss:
        return "deadline_miss";
    case EventType::MessageSend:
    case EventType::MessageDelivery:
        return std::nullopt;
    }
    throw std::logic_error{"unknown CPSSim event type"};
}

/*** Reads the pinned scheduler table and discards MATLAB-only row identities. ***/
std::vector<SchedulerObservation>
load_expected_scheduler(const std::filesystem::path& scenario_path) {
    const auto path = scenario_path / "scheduler_events.csv";
    auto input = open_csv(path);
    require_header(input, path,
                   "sequence,eventTimeSec,eventTick,eventType,eventTaskId,eventTaskName,"
                   "eventJobId,eventJobRow,eventResource");

    std::vector<SchedulerObservation> observations;
    std::string line;
    std::size_t row_number = 1;
    while (std::getline(input, line)) {
        ++row_number;
        remove_trailing_carriage_return(line);
        const auto fields = split_csv_row(line, path, row_number);
        if (fields.size() != 9) {
            throw std::runtime_error{path.string() + ": expected 9 fields at row " +
                                     std::to_string(row_number)};
        }
        observations.push_back(
            {.tick = parse_tick(fields[2], path, row_number, "eventTick"),
             .type = fields[3],
             .task_id = TaskId{parse_unsigned(fields[4], path, row_number, "eventTaskId")},
             .task_name = fields[5],
             .job_id = JobId{parse_unsigned(fields[6], path, row_number, "eventJobId")},
             .resource_name = fields[8]});
    }
    return observations;
}

/*** Projects processed CPSSim lifecycle events into the same scheduler shape. ***/
std::vector<SchedulerObservation> project_actual_scheduler(const std::vector<Event>& trace,
                                                           const ReferenceExperiment& experiment) {
    std::vector<SchedulerObservation> observations;
    for (const auto& event : trace) {
        const auto type_name = scheduler_event_name(event.type());
        if (!type_name.has_value()) {
            continue;
        }
        const auto& entities = event.entities();
        if (!entities.task_id.has_value() || !entities.job_id.has_value() ||
            !entities.resource_id.has_value()) {
            throw std::logic_error{"scheduler event lacks normalized entity references"};
        }
        observations.push_back(
            {.tick = event.tick(),
             .type = std::string{*type_name},
             .task_id = *entities.task_id,
             .task_name = task_name_by_id(experiment.tasks, *entities.task_id),
             .job_id = *entities.job_id,
             .resource_name = resource_name_by_id(experiment.resources, *entities.resource_id)});
    }
    return observations;
}

/*** Returns the configured receiver for one reference message source. ***/
TaskId destination_for_source(const ReferenceExperiment& experiment, TaskId source_task_id) {
    for (const auto& route : experiment.config.message_routes()) {
        if (route.source_task_id == source_task_id) {
            return route.destination_task_id;
        }
    }
    throw std::runtime_error{"network reference names an unrouted source task"};
}

/*** Reads causal source and timing while excluding packet/trigger identities. ***/
std::vector<NetworkObservation> load_expected_network(const std::filesystem::path& scenario_path,
                                                      const ReferenceExperiment& experiment) {
    const auto path = scenario_path / "network_events.csv";
    auto input = open_csv(path);
    require_header(input, path,
                   "packetId,direction,sourceTaskName,sourceJobId,publishTick,sendTick,"
                   "receiveTick,sendColumn,receiveColumn,sendInTrace,receiveInTrace");

    std::vector<NetworkObservation> observations;
    std::string line;
    std::size_t row_number = 1;
    while (std::getline(input, line)) {
        ++row_number;
        remove_trailing_carriage_return(line);
        const auto fields = split_csv_row(line, path, row_number);
        if (fields.size() != 11) {
            throw std::runtime_error{path.string() + ": expected 11 fields at row " +
                                     std::to_string(row_number)};
        }
        const auto source_id = task_id_by_name(experiment.tasks, fields[2]);
        const auto destination_id = destination_for_source(experiment, source_id);
        observations.push_back(
            {.source_task_id = source_id,
             .source_task_name = fields[2],
             .source_job_id = JobId{parse_unsigned(fields[3], path, row_number, "sourceJobId")},
             .destination_task_id = destination_id,
             .destination_task_name = task_name_by_id(experiment.tasks, destination_id),
             .publish_tick = parse_tick(fields[4], path, row_number, "publishTick"),
             .send_tick = parse_tick(fields[5], path, row_number, "sendTick"),
             .delivery_tick = parse_tick(fields[6], path, row_number, "receiveTick"),
             .send_in_trace = parse_boolean(fields[9], path, row_number, "sendInTrace"),
             .delivery_in_trace = parse_boolean(fields[10], path, row_number, "receiveInTrace")});
    }
    return observations;
}

/*** Projects network-owned runtime messages into their causal timing shape. ***/
std::vector<NetworkObservation> project_actual_network(const FixedDelayNetwork& network,
                                                       const ReferenceExperiment& experiment) {
    std::vector<NetworkObservation> observations;
    observations.reserve(network.messages().size());
    for (const auto& message : network.messages()) {
        const auto source_id = message.source_job().task_id();
        const auto destination_id = message.destination_task_id();
        observations.push_back(
            {.source_task_id = source_id,
             .source_task_name = task_name_by_id(experiment.tasks, source_id),
             .source_job_id = message.source_job().job_id(),
             .destination_task_id = destination_id,
             .destination_task_name = task_name_by_id(experiment.tasks, destination_id),
             .publish_tick = message.publish_tick(),
             .send_tick = message.send_tick(),
             .delivery_tick = message.delivery_tick(),
             .send_in_trace = message.send_event_sequence().has_value(),
             .delivery_in_trace = message.delivery_event_sequence().has_value()});
    }
    return observations;
}

/*** Reads the pinned sparse Bosch trigger table without parsing display seconds. ***/
std::vector<TriggerObservation> load_expected_triggers(const std::filesystem::path& scenario_path) {
    const auto path = scenario_path / "trigger_events.csv";
    auto input = open_csv(path);
    require_header(input, path, "eventTick,eventTimeSec,triggerColumn,triggerName");

    std::vector<TriggerObservation> observations;
    std::string line;
    std::size_t row_number = 1;
    while (std::getline(input, line)) {
        ++row_number;
        remove_trailing_carriage_return(line);
        const auto fields = split_csv_row(line, path, row_number);
        if (fields.size() != 4) {
            throw std::runtime_error{path.string() + ": expected 4 fields at row " +
                                     std::to_string(row_number)};
        }
        const auto column = parse_unsigned(fields[2], path, row_number, "triggerColumn");
        if (column == 0 || column > 16) {
            throw std::runtime_error{path.string() + ": triggerColumn is outside 1..16 at row " +
                                     std::to_string(row_number)};
        }
        observations.push_back({.tick = parse_tick(fields[0], path, row_number, "eventTick"),
                                .column = static_cast<std::size_t>(column),
                                .name = fields[3]});
    }
    return observations;
}

/*** Projects generic canonical events through the Bosch-specific adapter. ***/
std::vector<TriggerObservation> project_actual_triggers(const std::vector<Event>& trace) {
    const auto triggers = encode_bosch_v10_triggers(trace);
    std::vector<TriggerObservation> observations;
    observations.reserve(triggers.size());
    for (const auto& event : triggers) {
        observations.push_back({.tick = event.tick,
                                .column = bosch_trigger_column(event.trigger),
                                .name = std::string{bosch_trigger_name(event.trigger)}});
    }
    return observations;
}

// Compares every normalized scheduler field explicitly.
bool equal_observation(const SchedulerObservation& left, const SchedulerObservation& right) {
    return left.tick == right.tick && left.type == right.type && left.task_id == right.task_id &&
           left.task_name == right.task_name && left.job_id == right.job_id &&
           left.resource_name == right.resource_name;
}

// Compares every normalized causal network field explicitly.
bool equal_observation(const NetworkObservation& left, const NetworkObservation& right) {
    return left.source_task_id == right.source_task_id &&
           left.source_task_name == right.source_task_name &&
           left.source_job_id == right.source_job_id &&
           left.destination_task_id == right.destination_task_id &&
           left.destination_task_name == right.destination_task_name &&
           left.publish_tick == right.publish_tick && left.send_tick == right.send_tick &&
           left.delivery_tick == right.delivery_tick && left.send_in_trace == right.send_in_trace &&
           left.delivery_in_trace == right.delivery_in_trace;
}

// Compares the exact canonical identity of one sparse Bosch trigger pulse.
bool equal_observation(const TriggerObservation& left, const TriggerObservation& right) {
    return left.tick == right.tick && left.column == right.column && left.name == right.name;
}

/*** Formats one scheduler record for a compact diagnostic. ***/
std::string describe(const SchedulerObservation& observation) {
    std::ostringstream output;
    output << "tick=" << observation.tick << " type=" << observation.type
           << " task=" << observation.task_id.value() << '(' << observation.task_name << ')'
           << " job=" << observation.job_id.value() << " resource=" << observation.resource_name;
    return output.str();
}

/*** Formats one causal network record for a compact diagnostic. ***/
std::string describe(const NetworkObservation& observation) {
    std::ostringstream output;
    output << "source=" << observation.source_task_id.value() << '(' << observation.source_task_name
           << ")/job=" << observation.source_job_id.value()
           << " destination=" << observation.destination_task_id.value() << '('
           << observation.destination_task_name << ") publish=" << observation.publish_tick
           << " send=" << observation.send_tick << " delivery=" << observation.delivery_tick
           << " send_in_trace=" << (observation.send_in_trace ? 1 : 0)
           << " delivery_in_trace=" << (observation.delivery_in_trace ? 1 : 0);
    return output.str();
}

/*** Formats one Bosch trigger record for a compact diagnostic. ***/
std::string describe(const TriggerObservation& observation) {
    std::ostringstream output;
    output << "tick=" << observation.tick << " column=" << observation.column
           << " name=" << observation.name;
    return output.str();
}

/*** Produces the required first-row mismatch report for either stream. ***/
template <typename Observation>
std::string find_first_divergence(std::string_view stream, const std::vector<Observation>& expected,
                                  const std::vector<Observation>& actual) {
    const auto common_size = expected.size() < actual.size() ? expected.size() : actual.size();
    std::size_t index = 0;
    while (index < common_size && equal_observation(expected[index], actual[index])) {
        ++index;
    }
    if (index == expected.size() && index == actual.size()) {
        return {};
    }

    std::ostringstream output;
    output << "First divergence:\n"
           << "  stream: " << stream << '\n'
           << "  row: " << (index + 1) << '\n'
           << "  expected: "
           << (index < expected.size() ? describe(expected[index]) : std::string{"<missing>"})
           << '\n'
           << "  actual: "
           << (index < actual.size() ? describe(actual[index]) : std::string{"<missing>"}) << '\n'
           << "  previous matching row: ";
    if (index == 0) {
        output << "none";
    } else {
        output << index;
    }
    return output.str();
}

} // namespace

/*** Returns the directory and CLI spelling shared with the captured files. ***/
std::string_view bosch_reference_scenario_name(BoschReferenceScenario scenario) {
    switch (scenario) {
    case BoschReferenceScenario::Dedicated:
        return "dedicated";
    case BoschReferenceScenario::SharedCloud:
        return "shared_cloud";
    }
    throw std::logic_error{"unknown Bosch reference scenario"};
}

/*** Parses only the two task-defined scenario names. ***/
BoschReferenceScenario parse_bosch_reference_scenario(std::string_view name) {
    if (name == "dedicated") {
        return BoschReferenceScenario::Dedicated;
    }
    if (name == "shared_cloud") {
        return BoschReferenceScenario::SharedCloud;
    }
    throw std::invalid_argument{"scenario must be dedicated or shared_cloud"};
}

/*** Runs one normal engine and compares all three normalized observable streams. ***/
ConformanceReport compare_bosch_reference(const std::filesystem::path& reference_root,
                                          BoschReferenceScenario scenario) {
    const auto scenario_path = reference_root / bosch_reference_scenario_name(scenario);
    auto experiment = load_reference_experiment(scenario_path);
    const auto expected_scheduler = load_expected_scheduler(scenario_path);
    const auto expected_network = load_expected_network(scenario_path, experiment);
    const auto expected_triggers = load_expected_triggers(scenario_path);

    ConfiguredResourceAllocator allocator{experiment.assignments};
    FixedPriorityPolicy policy;
    SimulationEngine engine{experiment.config, reference_stop_tick, allocator, policy};
    engine.run();

    const auto actual_scheduler = project_actual_scheduler(engine.trace(), experiment);
    const auto actual_network = project_actual_network(engine.network(), experiment);
    const auto actual_triggers = project_actual_triggers(engine.trace());
    auto divergence =
        find_first_divergence("scheduler_events", expected_scheduler, actual_scheduler);
    if (divergence.empty()) {
        divergence = find_first_divergence("network_events", expected_network, actual_network);
    }
    if (divergence.empty()) {
        divergence = find_first_divergence("trigger_events", expected_triggers, actual_triggers);
    }

    return {.scenario = scenario,
            .matches = divergence.empty(),
            .expected_scheduler_events = expected_scheduler.size(),
            .actual_scheduler_events = actual_scheduler.size(),
            .expected_network_events = expected_network.size(),
            .actual_network_events = actual_network.size(),
            .expected_trigger_events = expected_triggers.size(),
            .actual_trigger_events = actual_triggers.size(),
            .first_divergence = std::move(divergence)};
}

/*** Reuses the pinned task/mapping loader for direct online model coupling. ***/
BoschOnlineRun run_bosch_reference_online(const std::filesystem::path& reference_root,
                                          BoschReferenceScenario scenario,
                                          FunctionalModel& functional_model, Tick stop_tick) {
    const auto scenario_path = reference_root / bosch_reference_scenario_name(scenario);
    auto experiment = load_reference_experiment(scenario_path);
    ConfiguredResourceAllocator allocator{experiment.assignments};
    FixedPriorityPolicy policy;
    SimulationEngine engine{experiment.config, stop_tick, allocator, policy, functional_model};
    engine.run();
    return {.canonical_trace = engine.trace(), .functional_trace = engine.functional_trace()};
}

/*** Formats one stable terminal report for humans and CI failure logs. ***/
std::string format_conformance_report(const ConformanceReport& report) {
    std::ostringstream output;
    output << "Scenario: " << bosch_reference_scenario_name(report.scenario) << '\n'
           << "Scheduler events: expected " << report.expected_scheduler_events << ", actual "
           << report.actual_scheduler_events << '\n'
           << "Network events: expected " << report.expected_network_events << ", actual "
           << report.actual_network_events << '\n'
           << "Trigger events: expected " << report.expected_trigger_events << ", actual "
           << report.actual_trigger_events << '\n'
           << "Result: " << (report.matches ? "PASS" : "FAIL");
    if (!report.first_divergence.empty()) {
        output << '\n' << report.first_divergence;
    }
    return output.str();
}

} // namespace cpssim
