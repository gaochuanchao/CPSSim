/*** Define presentation-only GUI workspace preferences and layout helpers. ***/

#pragma once

#include "cpssim/gui/signal_series.hpp"
#include "cpssim/model/categories.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

inline constexpr std::uint32_t current_gui_workspace_schema_version = 2;

enum class GuiTheme { Dark, Light };
enum class GuiAnalysisTab { Architecture, Timeline, Signals };
enum class GuiResourceTab { ResourceState, Utilization };

struct GuiPanelVisibility {
    bool explorer{true};
    bool system_builder{true};
    bool inspector{true};
    bool architecture{true};
    bool timeline{true};
    bool signals{true};
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
    float analysis_lower_ratio{0.56F};
    float resources_events_ratio{0.42F};
    GuiAnalysisTab active_analysis_tab{GuiAnalysisTab::Architecture};
    GuiResourceTab active_resource_tab{GuiResourceTab::ResourceState};
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
GuiClearColor gui_theme_clear_color(GuiTheme theme) noexcept;

} // namespace cpssim
