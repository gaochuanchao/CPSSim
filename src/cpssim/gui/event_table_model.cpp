/*** Implement immutable canonical-event table presentation. ***/

#include "cpssim/gui/event_table_model.hpp"

#include "cpssim/trace/event_json.hpp"

#include <algorithm>
#include <cctype>

namespace cpssim {

const char* gui_event_type_name(EventType type) noexcept {
    switch (type) {
    case EventType::JobRelease:
        return "Job release";
    case EventType::JobStart:
        return "Job start";
    case EventType::JobPreempt:
        return "Job preempt";
    case EventType::JobResume:
        return "Job resume";
    case EventType::JobFinish:
        return "Job finish";
    case EventType::DeadlineMiss:
        return "Deadline miss";
    case EventType::MessageSend:
        return "Message send";
    case EventType::MessageDelivery:
        return "Message delivery";
    }
    return "Unknown";
}

const char* gui_event_phase_name(EventPhase phase) noexcept {
    switch (phase) {
    case EventPhase::ExecutionCompletion:
        return "Execution completion";
    case EventPhase::MessageDelivery:
        return "Message delivery";
    case EventPhase::DeadlineCheck:
        return "Deadline check";
    case EventPhase::JobRelease:
        return "Job release";
    case EventPhase::PolicyUpdate:
        return "Policy update";
    case EventPhase::Scheduling:
        return "Scheduling";
    case EventPhase::CausedAction:
        return "Caused action";
    }
    return "Unknown";
}

std::vector<GuiEventTableRow> build_event_table_rows(const SimulationSnapshot& snapshot) {
    std::vector<GuiEventTableRow> rows;
    rows.reserve(snapshot.event_log.size());
    const auto tick_ms = static_cast<double>(snapshot.experiment.tick_period.count()) / 1'000'000.0;
    for (const auto& event : snapshot.event_log) {
        auto raw_json = serialize_event_json_line(event);
        if (!raw_json.empty() && raw_json.back() == '\n') {
            raw_json.pop_back();
        }
        rows.push_back({.sequence = event.sequence(),
                        .tick = event.tick(),
                        .time_milliseconds = static_cast<double>(event.tick()) * tick_ms,
                        .type = event.type(),
                        .phase = event.phase(),
                        .entities = event.entities(),
                        .cause = event.cause_sequence(),
                        .type_name = gui_event_type_name(event.type()),
                        .phase_name = gui_event_phase_name(event.phase()),
                        .raw_json = std::move(raw_json)});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
        return left.sequence < right.sequence;
    });
    return rows;
}

std::vector<std::size_t> filter_event_table_rows(const std::vector<GuiEventTableRow>& rows,
                                                 const GuiEventFilters& filters) {
    auto needle = filters.text;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    std::vector<std::size_t> result;
    result.reserve(rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto& row = rows[index];
        if ((filters.type.has_value() && row.type != *filters.type) ||
            (filters.task.has_value() && row.entities.task_id != filters.task) ||
            (filters.resource.has_value() && row.entities.resource_id != filters.resource) ||
            (filters.vehicle.has_value() && row.entities.vehicle_id != filters.vehicle)) {
            continue;
        }
        auto haystack = row.raw_json + ' ' + row.type_name + ' ' + row.phase_name;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (!needle.empty() && haystack.find(needle) == std::string::npos) {
            continue;
        }
        result.push_back(index);
    }
    return result;
}

std::optional<std::size_t> find_event_row_by_sequence(const std::vector<GuiEventTableRow>& rows,
                                                      EventSequence sequence) noexcept {
    const auto found =
        std::lower_bound(rows.begin(), rows.end(), sequence,
                         [](const auto& row, EventSequence value) { return row.sequence < value; });
    return found != rows.end() && found->sequence == sequence
               ? std::optional<std::size_t>{static_cast<std::size_t>(found - rows.begin())}
               : std::nullopt;
}

} // namespace cpssim
