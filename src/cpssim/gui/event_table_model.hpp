/*** Define canonical-event row projection, filtering, and cause navigation. ***/

#pragma once

#include "cpssim/gui/simulation_controller.hpp"
#include "cpssim/gui/workspace_state.hpp"

#include <optional>
#include <chrono>
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
    std::string search_text;
};

const char* gui_event_type_name(EventType type) noexcept;
const char* gui_event_phase_name(EventPhase phase) noexcept;
std::vector<GuiEventTableRow> build_event_table_rows(const SimulationSnapshot& snapshot);
std::vector<std::size_t> filter_event_table_rows(const std::vector<GuiEventTableRow>& rows,
                                                 const GuiEventFilters& filters);
std::optional<std::size_t> find_event_row_by_sequence(const std::vector<GuiEventTableRow>& rows,
                                                      EventSequence sequence) noexcept;
std::string event_raw_json(const SimulationSnapshot& snapshot, EventSequence sequence);

class GuiEventTableCache {
  public:
    bool update_rows(std::uint64_t presentation_generation,
                     const SimulationSnapshot& snapshot);
    bool update_filter(const GuiEventFilters& filters,
                       std::chrono::steady_clock::time_point now);
    const std::vector<GuiEventTableRow>& rows() const noexcept { return rows_; }
    const std::vector<std::size_t>& filtered_indices() const noexcept { return filtered_; }
    std::uint64_t row_build_count() const noexcept { return row_build_count_; }
    std::uint64_t filter_build_count() const noexcept { return filter_build_count_; }

  private:
    void rebuild_filter();
    std::optional<std::uint64_t> presentation_generation_;
    std::vector<GuiEventTableRow> rows_;
    std::vector<std::size_t> filtered_;
    GuiEventFilters effective_filters_;
    std::string pending_text_;
    std::chrono::steady_clock::time_point pending_text_since_{};
    bool filter_initialized_{false};
    std::uint64_t row_build_count_{};
    std::uint64_t filter_build_count_{};
};

} // namespace cpssim
