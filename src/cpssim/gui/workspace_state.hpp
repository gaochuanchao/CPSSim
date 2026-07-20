/*** Define presentation-only GUI workspace preferences and layout helpers. ***/

#pragma once

#include "cpssim/gui/signal_series.hpp"
#include "cpssim/gui/simulation_controller.hpp"
#include "cpssim/model/categories.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

inline constexpr std::uint32_t current_gui_workspace_schema_version = 4;

enum class GuiTheme { Dark, Light };
enum class GuiCenterTab { Architecture, Timeline, Signals, Results, Resources, Events };
enum class GuiPlotXAxisUnit { Ticks, Seconds };
enum class GuiPlotRangeMode { Full, Selected, Custom };

struct GuiPanelVisibility {
    bool explorer{true};
    bool system_builder{true};
    bool inspector{true};
    bool architecture{true};
    bool timeline{true};
    bool signals{true};
    bool results{true};
    bool resources{true};
    bool events{true};

    bool operator==(const GuiPanelVisibility&) const = default;
};

struct GuiEventColumnVisibility {
    bool sequence{true};
    bool tick{true};
    bool time{true};
    bool type{true};
    bool phase{true};
    bool task{true};
    bool job{true};
    bool resource{true};
    bool message{true};
    bool vehicle{true};
    bool cause{true};

    bool operator==(const GuiEventColumnVisibility&) const = default;
};

struct GuiEventFilters {
    std::optional<EventType> type;
    std::optional<TaskId> task;
    std::optional<ResourceId> resource;
    std::optional<VehicleId> vehicle;
    std::string text;

    bool operator==(const GuiEventFilters&) const = default;
};

/*** Contains no simulator, run-plan, draft, or runtime state. ***/
struct GuiWorkspaceState {
    std::uint32_t schema_version{current_gui_workspace_schema_version};
    GuiTheme theme{GuiTheme::Dark};
    GuiPanelVisibility panels;
    float left_sidebar_ratio{0.52F};
    float right_sidebar_ratio{0.48F};
    float center_split_ratio{0.62F};
    std::vector<GuiCenterTab> upper_tabs{GuiCenterTab::Architecture, GuiCenterTab::Timeline,
                                         GuiCenterTab::Signals, GuiCenterTab::Results};
    std::vector<GuiCenterTab> lower_tabs{GuiCenterTab::Resources, GuiCenterTab::Events};
    GuiCenterTab active_upper_tab{GuiCenterTab::Architecture};
    GuiCenterTab active_lower_tab{GuiCenterTab::Resources};
    GuiRunMode run_mode{GuiRunMode::Live};
    GuiFastBatchUnit fast_batch_unit{GuiFastBatchUnit::Events};
    std::uint64_t fast_event_batch_size{1000};
    std::uint64_t fast_tick_batch_size{1000};
    GuiPlotXAxisUnit plot_x_axis_unit{GuiPlotXAxisUnit::Ticks};
    GuiPlotRangeMode plot_range_mode{GuiPlotRangeMode::Full};
    Tick plot_custom_begin{0};
    Tick plot_custom_end{0};
    bool plot_auto_y{true};
    double plot_y_min{-1.0};
    double plot_y_max{1.0};
    bool plot_grid{true};
    bool plot_legend{true};
    float plot_line_thickness{1.5F};
    bool plot_markers{false};
    bool plot_bosch_thresholds{true};
    bool plot_bosch_critical_sections{true};
    bool plot_bosch_deadline_misses{true};
    bool plot_selected_tick{true};
    GuiEventFilters event_filters;
    GuiEventColumnVisibility event_columns;
    std::vector<GuiSignalId> selected_signals;

    bool operator==(const GuiWorkspaceState&) const = default;
};

struct GuiVerticalSplit {
    float first_height;
    float second_height;
    float normalized_ratio;

    bool operator==(const GuiVerticalSplit&) const = default;
};

struct GuiClearColor {
    float red;
    float green;
    float blue;
    float alpha;

    bool operator==(const GuiClearColor&) const = default;
};

float normalize_splitter_ratio(float ratio, float fallback) noexcept;
GuiVerticalSplit calculate_vertical_split(float available_height, float splitter_height,
                                          float desired_ratio, float minimum_first,
                                          float minimum_second) noexcept;
void normalize_workspace_state(GuiWorkspaceState& workspace) noexcept;
void reset_center_tab_arrangement(GuiWorkspaceState& workspace);
bool move_center_tab(GuiWorkspaceState& workspace, GuiCenterTab tab, bool to_upper);
bool center_tab_is_upper(const GuiWorkspaceState& workspace, GuiCenterTab tab) noexcept;
GuiClearColor gui_theme_clear_color(GuiTheme theme) noexcept;

} // namespace cpssim
