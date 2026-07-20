/*** Write standards-compatible XLSX workbooks with deterministic sheets and row splitting. ***/

#include "cpssim/application/results_workbook.hpp"

#include "cpssim/analysis/run_result.hpp"

#include <xlsxwriter.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace cpssim {
namespace {

struct WorkbookGuard {
    lxw_workbook* workbook{};
    bool closed{false};

    ~WorkbookGuard() {
        if (workbook != nullptr && !closed) {
            static_cast<void>(workbook_close(workbook));
        }
    }
};

void check(lxw_error error, std::string_view action) {
    if (error != LXW_NO_ERROR) {
        throw std::runtime_error{"Excel export failed while " + std::string{action} + ": " +
                                 lxw_strerror(error)};
    }
}

void text(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t column, std::string_view value,
          lxw_format* format = nullptr) {
    const auto owned = std::string{value};
    check(worksheet_write_string(sheet, row, column, owned.c_str(), format), "writing text");
}

template <typename Integer>
void exact_integer(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t column, Integer value,
                   lxw_format* format = nullptr) {
    text(sheet, row, column, std::to_string(value), format);
}

void number(lxw_worksheet* sheet, lxw_row_t row, lxw_col_t column, double value,
            lxw_format* format = nullptr) {
    check(worksheet_write_number(sheet, row, column, value, format), "writing a number");
}

void header(lxw_worksheet* sheet, const std::vector<std::string_view>& columns,
            lxw_format* format) {
    for (std::size_t index = 0; index < columns.size(); ++index) {
        text(sheet, 0, static_cast<lxw_col_t>(index), columns[index], format);
    }
    worksheet_freeze_panes(sheet, 1, 0);
}

double seconds(Tick tick, PhysicalDuration period) {
    return static_cast<double>(tick) * static_cast<double>(period.count()) / 1.0e9;
}

double seconds(double ticks, PhysicalDuration period) {
    return ticks * static_cast<double>(period.count()) / 1.0e9;
}

bool selected(Tick tick, const std::optional<GuiTickRange>& range) {
    return !range.has_value() || range->contains(tick);
}

std::vector<const Event*> selected_events(const RunResult& result,
                                          const std::optional<GuiTickRange>& range) {
    std::vector<const Event*> rows;
    for (const auto& event : result.snapshot.event_log) {
        if (selected(event.tick(), range)) {
            rows.push_back(&event);
        }
    }
    return rows;
}

struct SignalRow {
    const GuiSignalSeries* series;
    const GuiScalarSample* sample;
};

std::vector<SignalRow> selected_signals(const RunResult& result,
                                        const std::optional<GuiTickRange>& range) {
    if (!result.signals.model.has_value() || !result.signals.diagnostics.empty()) {
        throw std::invalid_argument{"functional signals are invalid and cannot be exported"};
    }
    const auto& model = result.signals.model.value();
    std::vector<SignalRow> rows;
    for (const auto& series : model.series) {
        for (const auto& sample : series.samples) {
            if (selected(sample.tick, range)) {
                rows.push_back({&series, &sample});
            }
        }
    }
    return rows;
}

void write_summary(lxw_workbook* workbook, const ProjectContext& project, const RunResult& result,
                   lxw_format* bold) {
    auto* sheet = workbook_add_worksheet(workbook, "Run Summary");
    header(sheet, {"Property", "Value"}, bold);
    const std::array<std::pair<std::string, std::string>, 8> rows{{
        {"Project", project.metadata().name},
        {"Scenario", result.scenario_kind},
        {"Run state", result.snapshot.run_state == GuiRunState::Finished ? "finished" : "paused"},
        {"Horizon tick", std::to_string(result.metrics.horizon_tick)},
        {"Event count", std::to_string(result.metrics.event_count)},
        {"Completed jobs", std::to_string(result.metrics.completed_jobs)},
        {"Deadline misses", std::to_string(result.metrics.deadline_misses)},
        {"Preemptions", std::to_string(result.metrics.preemptions)},
    }};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        text(sheet, static_cast<lxw_row_t>(index + 1), 0, rows[index].first);
        text(sheet, static_cast<lxw_row_t>(index + 1), 1, rows[index].second);
    }
    worksheet_set_column(sheet, 0, 0, 22.0, nullptr);
    worksheet_set_column(sheet, 1, 1, 28.0, nullptr);
}

void write_system(lxw_workbook* workbook, const ProjectContext& project, const RunResult& result,
                  lxw_format* bold) {
    const auto& config = project.session().config();
    auto* system = workbook_add_worksheet(workbook, "System");
    header(system, {"Property", "Value"}, bold);
    text(system, 1, 0, "Tick period (ns)");
    exact_integer(system, 1, 1, config.tick_period().count());
    text(system, 2, 0, "Preemption");
    text(system, 2, 1,
         config.scheduling().preemption_mode == PreemptionMode::Preemptive ? "preemptive"
                                                                           : "non_preemptive");

    auto* tasks = workbook_add_worksheet(workbook, "Tasks");
    header(tasks, {"ID", "Name", "Period ticks", "Deadline ticks", "Offset ticks", "Priority"},
           bold);
    for (std::size_t index = 0; index < config.tasks().size(); ++index) {
        const auto& task = config.tasks()[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        exact_integer(tasks, row, 0, task.id().value());
        text(tasks, row, 1, task.name());
        exact_integer(tasks, row, 2, task.period());
        exact_integer(tasks, row, 3, task.deadline());
        exact_integer(tasks, row, 4, task.offset());
        exact_integer(tasks, row, 5, task.priority());
    }

    auto* resources = workbook_add_worksheet(workbook, "Resources");
    header(resources, {"ID", "Name", "Busy ticks", "Idle ticks", "Utilization"}, bold);
    for (std::size_t index = 0; index < result.metrics.resources.size(); ++index) {
        const auto& resource = result.metrics.resources[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        exact_integer(resources, row, 0, resource.resource_id.value());
        text(resources, row, 1, resource.resource_name);
        exact_integer(resources, row, 2, resource.busy_ticks);
        exact_integer(resources, row, 3, resource.idle_ticks);
        if (resource.utilization.has_value()) {
            number(resources, row, 4, *resource.utilization);
        } else {
            text(resources, row, 4, "Unavailable");
        }
    }
}

void write_events(lxw_workbook* workbook, const RunResult& result,
                  const std::optional<GuiTickRange>& range, lxw_format* bold) {
    const auto rows = selected_events(result, range);
    for (const auto& part : plan_workbook_detail_sheets("Events", rows.size())) {
        auto* sheet = workbook_add_worksheet(workbook, part.name.c_str());
        header(sheet,
               {"Sequence", "Tick", "Time (s)", "Type", "Phase", "Task", "Job", "Resource",
                "Message", "Vehicle", "Cause"},
               bold);
        for (std::uint64_t offset = 0; offset < part.source_row_count; ++offset) {
            const auto& event = *rows[static_cast<std::size_t>(part.first_source_row + offset)];
            const auto row = static_cast<lxw_row_t>(offset + 1);
            exact_integer(sheet, row, 0, event.sequence().value());
            exact_integer(sheet, row, 1, event.tick());
            number(sheet, row, 2, seconds(event.tick(), result.snapshot.experiment.tick_period));
            text(sheet, row, 3, canonical_event_type_name(event.type()));
            text(sheet, row, 4, canonical_event_phase_name(event.phase()));
            const auto& entities = event.entities();
            if (entities.task_id)
                exact_integer(sheet, row, 5, entities.task_id->value());
            if (entities.job_id)
                exact_integer(sheet, row, 6, entities.job_id->value());
            if (entities.resource_id)
                exact_integer(sheet, row, 7, entities.resource_id->value());
            if (entities.message_id)
                exact_integer(sheet, row, 8, entities.message_id->value());
            if (entities.vehicle_id)
                exact_integer(sheet, row, 9, entities.vehicle_id->value());
            if (const auto cause = event.cause_sequence(); cause.has_value())
                exact_integer(sheet, row, 10, cause.value().value());
        }
    }
}

void write_signals(lxw_workbook* workbook, const RunResult& result,
                   const std::optional<GuiTickRange>& range, lxw_format* bold) {
    const auto rows = selected_signals(result, range);
    for (const auto& part : plan_workbook_detail_sheets("Functional Signals", rows.size())) {
        auto* sheet = workbook_add_worksheet(workbook, part.name.c_str());
        header(sheet,
               {"Tick", "Time (s)", "Type", "Source name", "Path", "Display name", "Unit", "Value"},
               bold);
        for (std::uint64_t offset = 0; offset < part.source_row_count; ++offset) {
            const auto& source = rows[static_cast<std::size_t>(part.first_source_row + offset)];
            const auto row = static_cast<lxw_row_t>(offset + 1);
            exact_integer(sheet, row, 0, source.sample->tick);
            number(sheet, row, 1,
                   seconds(source.sample->tick, result.snapshot.experiment.tick_period));
            const auto type = source.series->descriptor.id.scalar_type;
            text(sheet, row, 2,
                 type == GuiSignalScalarType::Real      ? "real"
                 : type == GuiSignalScalarType::Integer ? "integer"
                                                        : "boolean");
            text(sheet, row, 3, source.series->descriptor.id.source_name);
            text(sheet, row, 4, source.series->descriptor.path);
            text(sheet, row, 5, source.series->descriptor.display_name);
            text(sheet, row, 6, source.series->descriptor.unit);
            std::visit(
                [&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, double>) {
                        number(sheet, row, 7, value);
                    } else if constexpr (std::is_same_v<Value, bool>) {
                        check(worksheet_write_boolean(sheet, row, 7, value ? 1 : 0, nullptr),
                              "writing a boolean");
                    } else {
                        exact_integer(sheet, row, 7, value);
                    }
                },
                source.sample->value);
        }
    }
}

void write_metrics(lxw_workbook* workbook, const RunMetrics& metrics, lxw_format* bold) {
    auto* sheet = workbook_add_worksheet(workbook, "Scheduling Metrics");
    header(sheet,
           {"Category", "ID", "Name", "Metric", "Count", "Minimum ticks", "Mean ticks",
            "Maximum ticks", "Minimum (s)", "Mean (s)", "Maximum (s)", "Value"},
           bold);
    lxw_row_t row = 1;
    for (const auto& task : metrics.task_responses) {
        text(sheet, row, 0, "task");
        exact_integer(sheet, row, 1, task.task_id.value());
        text(sheet, row, 2, task.task_name);
        text(sheet, row, 3, "response_time");
        if (task.response_time) {
            exact_integer(sheet, row, 4, task.response_time->count);
            exact_integer(sheet, row, 5, task.response_time->minimum);
            number(sheet, row, 6, task.response_time->mean());
            exact_integer(sheet, row, 7, task.response_time->maximum);
            number(sheet, row, 8, seconds(task.response_time->minimum, metrics.tick_period));
            number(sheet, row, 9, seconds(task.response_time->mean(), metrics.tick_period));
            number(sheet, row, 10, seconds(task.response_time->maximum, metrics.tick_period));
        } else {
            text(sheet, row, 11, "Unavailable");
        }
        ++row;
    }
    text(sheet, row, 0, "message");
    text(sheet, row, 3, "delivery_delay");
    if (metrics.messages.delivery_delay) {
        exact_integer(sheet, row, 4, metrics.messages.delivery_delay->count);
        exact_integer(sheet, row, 5, metrics.messages.delivery_delay->minimum);
        number(sheet, row, 6, metrics.messages.delivery_delay->mean());
        exact_integer(sheet, row, 7, metrics.messages.delivery_delay->maximum);
        number(sheet, row, 8,
               seconds(metrics.messages.delivery_delay->minimum, metrics.tick_period));
        number(sheet, row, 9,
               seconds(metrics.messages.delivery_delay->mean(), metrics.tick_period));
        number(sheet, row, 10,
               seconds(metrics.messages.delivery_delay->maximum, metrics.tick_period));
    } else {
        text(sheet, row, 11, "Unavailable");
    }
}

void write_control_metrics(lxw_workbook* workbook,
                           const std::vector<WorkbookControlMetric>& metrics, lxw_format* bold) {
    if (metrics.empty()) {
        return;
    }
    auto* sheet = workbook_add_worksheet(workbook, "Control Metrics");
    header(sheet, {"Metric", "Tick", "Value"}, bold);
    for (std::size_t index = 0; index < metrics.size(); ++index) {
        const auto row = static_cast<lxw_row_t>(index + 1);
        text(sheet, row, 0, metrics[index].metric);
        if (const auto tick = metrics[index].tick; tick.has_value())
            exact_integer(sheet, row, 1, tick.value());
        text(sheet, row, 2, metrics[index].value);
    }
}

} // namespace

std::vector<WorkbookDetailSheet> plan_workbook_detail_sheets(const std::string& base_name,
                                                             std::uint64_t source_rows) {
    constexpr auto rows_per_sheet = excel_maximum_rows - 1;
    const auto sheet_count =
        std::max<std::uint64_t>(1, (source_rows + rows_per_sheet - 1) / rows_per_sheet);
    std::vector<WorkbookDetailSheet> result;
    result.reserve(static_cast<std::size_t>(sheet_count));
    for (std::uint64_t index = 0; index < sheet_count; ++index) {
        const auto first = index * rows_per_sheet;
        const auto count = std::min(rows_per_sheet, source_rows - std::min(first, source_rows));
        result.push_back(
            {.name = index == 0 ? base_name : base_name + " " + std::to_string(index + 1),
             .first_source_row = first,
             .source_row_count = count});
    }
    return result;
}

void write_results_workbook(const std::filesystem::path& path, const ProjectContext& project,
                            const RunResult& result, std::optional<GuiTickRange> range,
                            const std::vector<WorkbookControlMetric>& control_metrics) {
    WorkbookGuard guard{.workbook = workbook_new(path.string().c_str())};
    if (guard.workbook == nullptr) {
        throw std::runtime_error{"cannot create Excel workbook '" + path.string() + "'"};
    }
    auto* bold = workbook_add_format(guard.workbook);
    format_set_bold(bold);
    write_summary(guard.workbook, project, result, bold);
    write_system(guard.workbook, project, result, bold);
    write_events(guard.workbook, result, range, bold);
    write_signals(guard.workbook, result, range, bold);
    write_metrics(guard.workbook, result.metrics, bold);
    write_control_metrics(guard.workbook, control_metrics, bold);
    const auto error = workbook_close(guard.workbook);
    guard.closed = true;
    check(error, "closing the workbook");
}

} // namespace cpssim
