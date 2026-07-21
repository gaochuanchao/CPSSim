/***
 * File: apps/gui/views/architecture_view.hpp
 * Purpose: Declare G04 draw-list rendering and workspace interaction state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/gui/architecture_graph.hpp"
#include "cpssim/gui/frame_scheduler.hpp"
#include "cpssim/gui/simulation_session.hpp"

#include <optional>
#include <string>

namespace cpssim::gui {

/*** Owns disposable workspace state; no value affects simulation behavior. ***/
struct ArchitectureViewState {
    bool fit_requested{true};
    std::optional<TaskId> pressed_task;
    std::optional<ResourceId> pressed_resource;
    bool resizing_resource_x{false};
    bool resizing_resource_y{false};
    bool dragging_task{false};
    std::string status;
    bool status_error{false};
};

/***
 * Draws one graph canvas and returns true when a double-click requests the
 * Inspector panel.
 ***/
bool draw_architecture_view(const GuiArchitectureGraph& graph,
                            const ExperimentPresentationSnapshot& experiment,
                            std::vector<DraftTaskAssignment>& assignments,
                            StructuralSelection& selection, GuiArchitectureWorkspace& layout,
                            ArchitectureViewState& state, bool editing_enabled,
                            bool read_only_preview = false,
                            GuiPointerRegionMap* pointer_regions = nullptr);

} // namespace cpssim::gui
