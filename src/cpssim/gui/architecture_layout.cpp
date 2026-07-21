/*** Implement deterministic presentation-only Architecture layout overrides. ***/
#include "cpssim/gui/architecture_layout.hpp"

#include <algorithm>
#include <cmath>

namespace cpssim {
namespace {
GuiResourceLayoutOverride& resource_override(GuiArchitectureWorkspace& state, ResourceId id) {
    const auto found = std::find_if(state.resources.begin(), state.resources.end(),
                                    [id](const auto& row) { return row.resource_id == id; });
    if (found != state.resources.end()) {
        return *found;
    }
    state.resources.push_back({id, std::nullopt, std::nullopt});
    return state.resources.back();
}
} // namespace

void normalize_architecture_workspace(GuiArchitectureWorkspace& state) noexcept {
    if (!std::isfinite(state.pan_x))
        state.pan_x = 0.0F;
    if (!std::isfinite(state.pan_y))
        state.pan_y = 0.0F;
    state.zoom = std::isfinite(state.zoom) ? std::clamp(state.zoom, 0.025F, 2.5F) : 1.0F;
    std::erase_if(state.tasks, [](const auto& row) {
        return !std::isfinite(row.position.x) || !std::isfinite(row.position.y);
    });
    std::erase_if(state.resources, [](auto& row) {
        if (row.position.has_value() &&
            (!std::isfinite(row.position->x) || !std::isfinite(row.position->y)))
            row.position.reset();
        if (row.size.has_value() &&
            (!std::isfinite(row.size->width) || !std::isfinite(row.size->height) ||
             row.size->width < 48.0F || row.size->height < 36.0F))
            row.size.reset();
        return !row.position.has_value() && !row.size.has_value();
    });
}

void reset_architecture_layout(GuiArchitectureWorkspace& state) noexcept { state = {}; }
void auto_layout_architecture(GuiArchitectureWorkspace& state) noexcept {
    state.tasks.clear();
    state.resources.clear();
}
void set_task_layout_position(GuiArchitectureWorkspace& state, TaskId id, GuiLayoutPoint position) {
    const auto found = std::find_if(state.tasks.begin(), state.tasks.end(),
                                    [id](const auto& row) { return row.task_id == id; });
    if (found != state.tasks.end())
        found->position = position;
    else
        state.tasks.push_back({id, position});
}
void set_resource_layout_position(GuiArchitectureWorkspace& state, ResourceId id,
                                  GuiLayoutPoint position) {
    resource_override(state, id).position = position;
}
void set_resource_layout_size(GuiArchitectureWorkspace& state, ResourceId id, GuiLayoutSize size) {
    resource_override(state, id).size = size;
}
void reset_task_layout_position(GuiArchitectureWorkspace& state, TaskId id) {
    std::erase_if(state.tasks, [id](const auto& row) { return row.task_id == id; });
}
void reset_resource_layout_position(GuiArchitectureWorkspace& state, ResourceId id) {
    auto& row = resource_override(state, id);
    row.position.reset();
    if (!row.size.has_value())
        std::erase_if(state.resources, [id](const auto& value) { return value.resource_id == id; });
}
void reset_resource_layout_size(GuiArchitectureWorkspace& state, ResourceId id) {
    auto& row = resource_override(state, id);
    row.size.reset();
    if (!row.position.has_value())
        std::erase_if(state.resources, [id](const auto& value) { return value.resource_id == id; });
}
const GuiTaskLayoutOverride* find_task_layout(const GuiArchitectureWorkspace& state,
                                              TaskId id) noexcept {
    const auto found = std::find_if(state.tasks.begin(), state.tasks.end(),
                                    [id](const auto& row) { return row.task_id == id; });
    return found == state.tasks.end() ? nullptr : &*found;
}
const GuiResourceLayoutOverride* find_resource_layout(const GuiArchitectureWorkspace& state,
                                                      ResourceId id) noexcept {
    const auto found = std::find_if(state.resources.begin(), state.resources.end(),
                                    [id](const auto& row) { return row.resource_id == id; });
    return found == state.resources.end() ? nullptr : &*found;
}
} // namespace cpssim
