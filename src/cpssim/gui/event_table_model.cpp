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
        return "Caused action";    case EventPhase::CausedActionLate:
        return "caused_action_late";    }
    return "Unknown";
}

std::vector<GuiEventTableRow> build_event_table_rows(const SimulationSnapshot& snapshot) {
    std::vector<GuiEventTableRow> rows;
    rows.reserve(snapshot.event_log.size());
    const auto tick_ms = static_cast<double>(snapshot.experiment.tick_period.count()) / 1'000'000.0;
    for (const auto& event : snapshot.event_log) {
        auto search_text = std::string{gui_event_type_name(event.type())} + ' ' +
                           gui_event_phase_name(event.phase());
        const auto append_id = [&search_text](const auto& id) {
            if (id.has_value()) {
                search_text += ' ' + std::to_string(id->value());
            }
        };
        append_id(event.entities().task_id);
        append_id(event.entities().job_id);
        append_id(event.entities().resource_id);
        append_id(event.entities().message_id);
        append_id(event.entities().vehicle_id);
        std::transform(search_text.begin(), search_text.end(), search_text.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        rows.push_back({.sequence = event.sequence(),
                        .tick = event.tick(),
                        .time_milliseconds = static_cast<double>(event.tick()) * tick_ms,
                        .type = event.type(),
                        .phase = event.phase(),
                        .entities = event.entities(),
                        .cause = event.cause_sequence(),
                        .type_name = gui_event_type_name(event.type()),
                        .phase_name = gui_event_phase_name(event.phase()),
                        .search_text = std::move(search_text)});
    }
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
        if (!needle.empty() && row.search_text.find(needle) == std::string::npos) {
            continue;
        }
        result.push_back(index);
    }
    return result;
}

std::optional<std::size_t> find_event_row_by_sequence(const std::vector<GuiEventTableRow>& rows,
                                                      EventSequence sequence) noexcept {
    const auto found = std::find_if(
        rows.begin(), rows.end(), [sequence](const auto& row) { return row.sequence == sequence; });
    return found != rows.end() && found->sequence == sequence
               ? std::optional<std::size_t>{static_cast<std::size_t>(found - rows.begin())}
               : std::nullopt;
}

std::string event_raw_json(const SimulationSnapshot& snapshot, EventSequence sequence) {
    const auto found =
        std::find_if(snapshot.event_log.begin(), snapshot.event_log.end(),
                     [sequence](const auto& event) { return event.sequence() == sequence; });
    if (found == snapshot.event_log.end()) {
        return {};
    }
    auto result = serialize_event_json_line(*found);
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

bool GuiEventTableCache::update_rows(std::uint64_t presentation_generation,
                                     const SimulationSnapshot& snapshot) {
    if (presentation_generation_ == presentation_generation) {
        return false;
    }
    rows_ = build_event_table_rows(snapshot);
    presentation_generation_ = presentation_generation;
    ++row_build_count_;
    rebuild_filter();
    return true;
}

bool GuiEventTableCache::update_filter(const GuiEventFilters& filters,
                                       std::chrono::steady_clock::time_point now) {
    auto non_text = filters;
    non_text.text = effective_filters_.text;
    const auto non_text_changed = non_text != effective_filters_;
    if (non_text_changed) {
        effective_filters_.type = filters.type;
        effective_filters_.task = filters.task;
        effective_filters_.resource = filters.resource;
        effective_filters_.vehicle = filters.vehicle;
    }
    if (!filter_initialized_) {
        effective_filters_ = filters;
        pending_text_ = filters.text;
        filter_initialized_ = true;
        rebuild_filter();
        return true;
    }
    if (filters.text != pending_text_) {
        pending_text_ = filters.text;
        pending_text_since_ = now;
    }
    constexpr auto debounce = std::chrono::milliseconds{150};
    const auto text_ready =
        effective_filters_.text != pending_text_ && now - pending_text_since_ >= debounce;
    if (text_ready) {
        effective_filters_.text = pending_text_;
    }
    if (non_text_changed || text_ready) {
        rebuild_filter();
        return true;
    }
    return false;
}

void GuiEventTableCache::rebuild_filter() {
    filtered_ = filter_event_table_rows(rows_, effective_filters_);
    ++filter_build_count_;
}

} // namespace cpssim
