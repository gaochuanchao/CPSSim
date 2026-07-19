/***
 * File: src/cpssim/bosch/example_data.cpp
 * Purpose: Strictly load Bosch example time, position, feedforward, and
 *          velocity CSV columns into immutable functional-model input.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Decimal seconds are converted to 100-microsecond ticks exactly and
 *        are never used as canonical floating-point event timestamps.
 ***/

#include "cpssim/bosch/example_data.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cpssim {
namespace {

constexpr std::uint64_t ticks_per_second = 10'000;

struct InputColumn {
    std::filesystem::path path;
    std::ifstream stream;
};

/*** Removes the carriage return retained when Windows CSVs are read on Unix. ***/
void remove_trailing_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

/*** Parses a complete finite Real field with file and row diagnostics. ***/
double parse_finite_real(const std::string& text, const std::filesystem::path& path,
                         std::size_t row_number) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error{path.string() + ": invalid number at row " +
                                 std::to_string(row_number)};
    }
    if (consumed != text.size() || !std::isfinite(value)) {
        throw std::runtime_error{path.string() + ": invalid number at row " +
                                 std::to_string(row_number)};
    }
    return value;
}

/*** Parses nonnegative decimal seconds as an exact 100-microsecond Tick. ***/
Tick parse_time_tick(std::string_view text, const std::filesystem::path& path,
                     std::size_t row_number) {
    const auto decimal = text.find('.');
    if (text.empty() || decimal == 0 || text.find('.', decimal + 1) != std::string_view::npos) {
        throw std::runtime_error{path.string() + ": invalid time at row " +
                                 std::to_string(row_number)};
    }

    const auto whole_text = text.substr(0, decimal);
    const auto fraction_text =
        decimal == std::string_view::npos ? std::string_view{} : text.substr(decimal + 1);
    if (fraction_text.size() > 4) {
        throw std::runtime_error{path.string() + ": time is not on the 0.0001-second grid at row " +
                                 std::to_string(row_number)};
    }

    std::uint64_t whole = 0;
    const auto whole_result =
        std::from_chars(whole_text.data(), whole_text.data() + whole_text.size(), whole);
    if (whole_result.ec != std::errc{} ||
        whole_result.ptr != whole_text.data() + whole_text.size()) {
        throw std::runtime_error{path.string() + ": invalid time at row " +
                                 std::to_string(row_number)};
    }

    std::uint64_t fraction = 0;
    for (const char digit : fraction_text) {
        if (digit < '0' || digit > '9') {
            throw std::runtime_error{path.string() + ": invalid time at row " +
                                     std::to_string(row_number)};
        }
        fraction = fraction * 10 + static_cast<std::uint64_t>(digit - '0');
    }
    for (std::size_t digit = fraction_text.size(); digit < 4; ++digit) {
        fraction *= 10;
    }

    const auto max_tick = static_cast<std::uint64_t>(std::numeric_limits<Tick>::max());
    if (whole > (max_tick - fraction) / ticks_per_second) {
        throw std::runtime_error{path.string() + ": time exceeds Tick at row " +
                                 std::to_string(row_number)};
    }
    return static_cast<Tick>(whole * ticks_per_second + fraction);
}

/*** Opens one required input column before any rows are accepted. ***/
InputColumn open_column(const std::filesystem::path& example_directory, std::string_view filename) {
    auto path = example_directory / filename;
    std::ifstream stream{path};
    if (!stream) {
        throw std::runtime_error{"cannot open Bosch example CSV: " + path.string()};
    }
    return {.path = std::move(path), .stream = std::move(stream)};
}

} // namespace

/*** Reads all six Bosch columns in lockstep and retains the three FMI inputs. ***/
std::vector<BoschTrajectorySample>
load_bosch_example_trajectory(const std::filesystem::path& example_directory) {
    std::array columns{
        open_column(example_directory, "time_vector.csv"),
        open_column(example_directory, "feedforward_sequence_0.csv"),
        open_column(example_directory, "feedforward_sequence_1.csv"),
        open_column(example_directory, "velocity.csv"),
        open_column(example_directory, "x_position_track.csv"),
        open_column(example_directory, "y_position_track.csv"),
    };

    std::vector<BoschTrajectorySample> trajectory;
    trajectory.reserve(1'500'000);
    std::array<std::string, 6> rows;
    while (true) {
        std::array<bool, 6> present{};
        for (std::size_t column = 0; column < columns.size(); ++column) {
            present[column] = static_cast<bool>(std::getline(columns[column].stream, rows[column]));
        }
        if (std::none_of(present.begin(), present.end(), [](bool value) { return value; })) {
            break;
        }
        if (!std::all_of(present.begin(), present.end(), [](bool value) { return value; })) {
            throw std::runtime_error{"Bosch example CSV columns have different row counts in " +
                                     example_directory.string()};
        }

        const auto row_number = trajectory.size() + 1;
        for (auto& row : rows) {
            remove_trailing_carriage_return(row);
        }
        if (row_number - 1 > static_cast<std::size_t>(std::numeric_limits<Tick>::max())) {
            throw std::runtime_error{"Bosch example has more rows than Tick can represent"};
        }
        const auto expected_tick = static_cast<Tick>(row_number - 1);
        const auto actual_tick = parse_time_tick(rows[0], columns[0].path, row_number);
        if (actual_tick != expected_tick) {
            throw std::runtime_error{columns[0].path.string() + ": expected consecutive tick " +
                                     std::to_string(expected_tick) + " at row " +
                                     std::to_string(row_number)};
        }

        const auto feedforward_0 = parse_finite_real(rows[1], columns[1].path, row_number);
        const auto feedforward_1 = parse_finite_real(rows[2], columns[2].path, row_number);
        const auto velocity = parse_finite_real(rows[3], columns[3].path, row_number);
        static_cast<void>(parse_finite_real(rows[4], columns[4].path, row_number));
        static_cast<void>(parse_finite_real(rows[5], columns[5].path, row_number));
        if (velocity <= 0.0) {
            throw std::runtime_error{columns[3].path.string() +
                                     ": velocity must be positive at row " +
                                     std::to_string(row_number)};
        }
        trajectory.push_back(
            {.feedforward_0 = feedforward_0, .feedforward_1 = feedforward_1, .velocity = velocity});
    }

    if (trajectory.empty()) {
        throw std::runtime_error{"Bosch example trajectory is empty: " +
                                 example_directory.string()};
    }
    return trajectory;
}

} // namespace cpssim
