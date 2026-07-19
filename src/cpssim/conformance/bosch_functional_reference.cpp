/***
 * File: src/cpssim/conformance/bosch_functional_reference.cpp
 * Purpose: Execute and compare Bosch functional behavior online, offline, and
 *          against the immutable 15-second MATLAB/Simulink CSV capture.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Parsing is intentionally strict so malformed evidence cannot look
 *        like simulator divergence.
 ***/

#include "cpssim/conformance/bosch_functional_reference.hpp"

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"
#include "cpssim/functional/functional_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cpssim {
namespace {

constexpr Tick reference_stop_tick = 150'000;
constexpr PhysicalDuration reference_tick_period = std::chrono::microseconds{100};
constexpr FunctionalTolerance reference_tolerance{.absolute = 1e-12, .relative = 1e-9};

/*** Stores one strictly parsed expected reference row. ***/
struct ExpectedFunctionalRow {
    double time_seconds;
    std::array<double, 4> real_values;
    std::int64_t violation_counter;
    bool critical_section;
};

/*** Splits one comma-delimited line without silently dropping empty fields. ***/
std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= line.size()) {
        const auto comma = line.find(',', begin);
        fields.push_back(line.substr(begin, comma - begin));
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
    return fields;
}

/*** Parses one complete finite decimal field with contextual diagnostics. ***/
double parse_double(const std::string& text, std::string_view context) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error{std::string{context} + " contains an invalid number"};
    }
    if (consumed != text.size() || !std::isfinite(value)) {
        throw std::runtime_error{std::string{context} + " contains an invalid number"};
    }
    return value;
}

/*** Loads exactly the inclusive reference horizon from one-column input CSV. ***/
std::vector<double> load_trajectory_column(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot open Bosch trajectory: " + path.string()};
    }

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(reference_stop_tick + 1));
    std::string line;
    while (values.size() < static_cast<std::size_t>(reference_stop_tick + 1) &&
           std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        values.push_back(parse_double(line, path.filename().string()));
    }
    if (values.size() != static_cast<std::size_t>(reference_stop_tick + 1)) {
        throw std::runtime_error{"Bosch trajectory is shorter than the reference horizon: " +
                                 path.string()};
    }
    return values;
}

/*** Combines the three pinned environment streams by equal row index. ***/
std::vector<BoschTrajectorySample> load_trajectory(const std::filesystem::path& reference_root) {
    const auto input_root = reference_root.parent_path().parent_path() / "examples/example_v_10";
    const auto feedforward_0 = load_trajectory_column(input_root / "feedforward_sequence_0.csv");
    const auto feedforward_1 = load_trajectory_column(input_root / "feedforward_sequence_1.csv");
    const auto velocity = load_trajectory_column(input_root / "velocity.csv");

    std::vector<BoschTrajectorySample> trajectory;
    trajectory.reserve(feedforward_0.size());
    for (std::size_t index = 0; index < feedforward_0.size(); ++index) {
        trajectory.push_back({.feedforward_0 = feedforward_0[index],
                              .feedforward_1 = feedforward_1[index],
                              .velocity = velocity[index]});
    }
    return trajectory;
}

/*** Loads the immutable expected table and validates its stable seven columns. ***/
std::vector<ExpectedFunctionalRow> load_expected_output(const std::filesystem::path& reference_root,
                                                        BoschReferenceScenario scenario) {
    const auto path = reference_root / bosch_reference_scenario_name(scenario) / "fmu_outputs.csv";
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"cannot open Bosch functional reference: " + path.string()};
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error{"Bosch functional reference is empty: " + path.string()};
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    const std::string expected_header =
        "time_sec,lateral_error,actuator_command,rolling_real,rolling_remote,"
        "violation_counter,critical_section";
    if (line != expected_header) {
        throw std::runtime_error{"Bosch functional reference header is not recognized"};
    }

    std::vector<ExpectedFunctionalRow> rows;
    rows.reserve(static_cast<std::size_t>(reference_stop_tick + 1));
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto fields = split_csv(line);
        if (fields.size() != 7) {
            throw std::runtime_error{"Bosch functional reference row does not have seven fields"};
        }
        const double counter = parse_double(fields[5], "violation_counter");
        const double critical = parse_double(fields[6], "critical_section");
        if (std::floor(counter) != counter ||
            counter < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
            counter > static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
            (critical != 0.0 && critical != 1.0)) {
            throw std::runtime_error{"Bosch functional reference has invalid discrete output"};
        }
        rows.push_back({.time_seconds = parse_double(fields[0], "time_sec"),
                        .real_values = {parse_double(fields[1], "lateral_error"),
                                        parse_double(fields[2], "actuator_command"),
                                        parse_double(fields[3], "rolling_real"),
                                        parse_double(fields[4], "rolling_remote")},
                        .violation_counter = static_cast<std::int64_t>(counter),
                        .critical_section = critical == 1.0});
    }
    if (rows.size() != static_cast<std::size_t>(reference_stop_tick + 1)) {
        throw std::runtime_error{"Bosch functional reference does not cover the exact horizon"};
    }
    return rows;
}

/*** Compares names and values exactly for deterministic online/offline replay. ***/
bool observations_equal(const FunctionalObservation& left, const FunctionalObservation& right) {
    if (left.tick != right.tick || left.real_signals.size() != right.real_signals.size() ||
        left.integer_signals.size() != right.integer_signals.size() ||
        left.boolean_signals.size() != right.boolean_signals.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.real_signals.size(); ++index) {
        if (left.real_signals[index].name != right.real_signals[index].name ||
            left.real_signals[index].value != right.real_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.integer_signals.size(); ++index) {
        if (left.integer_signals[index].name != right.integer_signals[index].name ||
            left.integer_signals[index].value != right.integer_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.boolean_signals.size(); ++index) {
        if (left.boolean_signals[index].name != right.boolean_signals[index].name ||
            left.boolean_signals[index].value != right.boolean_signals[index].value) {
            return false;
        }
    }
    return true;
}

/*** Reports whether every online and replay row is bit-for-bit equal. ***/
bool traces_equal(const std::vector<FunctionalObservation>& online,
                  const std::vector<FunctionalObservation>& replay) {
    return online.size() == replay.size() &&
           std::equal(online.begin(), online.end(), replay.begin(), observations_equal);
}

/*** Returns one observation field only when its stable name and position agree. ***/
double required_real(const FunctionalObservation& observation, std::size_t index,
                     std::string_view name) {
    if (index >= observation.real_signals.size() || observation.real_signals[index].name != name) {
        throw std::runtime_error{"Bosch functional observation Real schema changed"};
    }
    return observation.real_signals[index].value;
}

} // namespace

/*** Runs online and replay FMI instances, then compares every captured row. ***/
BoschFunctionalConformanceReport
compare_bosch_functional_reference(const std::filesystem::path& reference_root,
                                   BoschReferenceScenario scenario,
                                   const Fmi2ModelInfo& model_info) {
    const auto trajectory = load_trajectory(reference_root);
    const auto expected = load_expected_output(reference_root, scenario);

    BoschFmi2FunctionalModel online_model{model_info, trajectory};
    const auto online = run_bosch_reference_online(reference_root, scenario, online_model);

    auto replay_info = model_info;
    replay_info.instance_name += "_replay";
    BoschFmi2FunctionalModel replay_model{replay_info, trajectory};
    const auto replay = replay_functional_trace(replay_model, reference_tick_period,
                                                reference_stop_tick, online.canonical_trace);

    BoschFunctionalConformanceReport report{.scenario = scenario,
                                            .matches = true,
                                            .online_replay_matches =
                                                traces_equal(online.functional_trace, replay),
                                            .expected_rows = expected.size(),
                                            .actual_rows = online.functional_trace.size(),
                                            .max_absolute_error = 0.0,
                                            .max_relative_error = 0.0,
                                            .tolerance = reference_tolerance,
                                            .first_divergence = {}};
    if (!report.online_replay_matches) {
        report.matches = false;
        report.first_divergence = "online and offline functional traces differ";
    }
    if (expected.size() != online.functional_trace.size()) {
        report.matches = false;
        if (report.first_divergence.empty()) {
            report.first_divergence = "functional trace row count differs";
        }
    }

    const std::array<std::string_view, 4> real_names{"lateral_error", "actuator_command",
                                                     "rolling_real", "rolling_remote"};
    const auto row_count = std::min(expected.size(), online.functional_trace.size());
    for (std::size_t row = 0; row < row_count; ++row) {
        const auto& actual = online.functional_trace[row];
        const auto& wanted = expected[row];
        const double expected_time =
            static_cast<double>(ticks_to_duration(actual.tick, reference_tick_period).count()) /
            1'000'000'000.0;
        bool row_matches = actual.tick == static_cast<Tick>(row) &&
                           std::abs(wanted.time_seconds - expected_time) <= 1e-12 &&
                           actual.integer_signals.size() == 1 &&
                           actual.integer_signals[0].name == "violation_counter" &&
                           actual.integer_signals[0].value == wanted.violation_counter &&
                           actual.boolean_signals.size() == 1 &&
                           actual.boolean_signals[0].name == "critical_section" &&
                           actual.boolean_signals[0].value == wanted.critical_section;
        std::string row_detail;

        for (std::size_t field = 0; field < real_names.size(); ++field) {
            const double actual_value = required_real(actual, field, real_names[field]);
            const double absolute_error = std::abs(actual_value - wanted.real_values[field]);
            const double relative_error =
                wanted.real_values[field] == 0.0
                    ? 0.0
                    : absolute_error / std::abs(wanted.real_values[field]);
            report.max_absolute_error = std::max(report.max_absolute_error, absolute_error);
            report.max_relative_error = std::max(report.max_relative_error, relative_error);
            if (absolute_error >
                reference_tolerance.absolute +
                    reference_tolerance.relative * std::abs(wanted.real_values[field])) {
                row_matches = false;
                if (row_detail.empty()) {
                    std::ostringstream detail;
                    detail << real_names[field] << " expected " << std::setprecision(17)
                           << wanted.real_values[field] << ", actual " << actual_value;
                    row_detail = detail.str();
                }
            }
        }

        if (!row_matches && report.first_divergence.empty()) {
            std::ostringstream detail;
            detail << "first functional divergence at row " << row << " (tick " << actual.tick
                   << ')';
            if (!row_detail.empty()) {
                detail << ": " << row_detail;
            }
            report.first_divergence = detail.str();
        }
        report.matches = report.matches && row_matches;
    }
    return report;
}

/*** Formats stable measured evidence for test logs and manual validation. ***/
std::string format_bosch_functional_report(const BoschFunctionalConformanceReport& report) {
    std::ostringstream output;
    output << "Scenario: " << bosch_reference_scenario_name(report.scenario) << '\n'
           << "Functional rows: expected " << report.expected_rows << ", actual "
           << report.actual_rows << '\n'
           << "Online/offline replay: " << (report.online_replay_matches ? "MATCH" : "DIFFER")
           << '\n'
           << std::setprecision(17) << "Maximum absolute Real error: " << report.max_absolute_error
           << '\n'
           << "Maximum relative Real error: " << report.max_relative_error << '\n'
           << "Tolerance: abs " << report.tolerance.absolute << " + rel "
           << report.tolerance.relative << " * |expected|\n"
           << "Result: " << (report.matches ? "PASS" : "FAIL");
    if (!report.first_divergence.empty()) {
        output << '\n' << report.first_divergence;
    }
    return output.str();
}

} // namespace cpssim
