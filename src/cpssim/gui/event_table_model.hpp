/*** Define canonical-event row projection, filtering, and cause navigation. ***/

#pragma once

#include "cpssim/gui/simulation_controller.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim {

struct GuiEventTableRow {
    EventSequence sequence;
    Tick tick;
    double time_milliseconds;
    EventType type;
    EventPhase phase;
    EventEntityRefs entities;
    std::optional<EventSequence> cause;
    std::string type_name;
    std::string phase_name;
    std::string raw_json;
};

const char* gui_event_type_name(EventType type) noexcept;
const char* gui_event_phase_name(EventPhase phase) noexcept;
std::vector<GuiEventTableRow> build_event_table_rows(const SimulationSnapshot& snapshot);
std::vector<std::size_t> filter_event_table_rows(const std::vector<GuiEventTableRow>& rows,
                                                 const GuiEventFilters& filters);
std::optional<std::size_t> find_event_row_by_sequence(const std::vector<GuiEventTableRow>& rows,
                                                      EventSequence sequence) noexcept;

} // namespace cpssim
