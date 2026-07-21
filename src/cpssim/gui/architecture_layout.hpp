/*** Presentation-only manual Architecture layout state and mutations. ***/
#pragma once

#include "cpssim/model/identifiers.hpp"

#include <optional>
#include <vector>

namespace cpssim {

enum class GuiArchitectureMode { Select, Arrange, Assign };

struct GuiLayoutPoint {
    float x{};
    float y{};
    bool operator==(const GuiLayoutPoint&) const = default;
};

struct GuiLayoutSize {
    float width{};
    float height{};
    bool operator==(const GuiLayoutSize&) const = default;
};

struct GuiTaskLayoutOverride {
    TaskId task_id;
    GuiLayoutPoint position;
    bool operator==(const GuiTaskLayoutOverride&) const = default;
};

struct GuiResourceLayoutOverride {
    ResourceId resource_id;
    std::optional<GuiLayoutPoint> position;
    std::optional<GuiLayoutSize> size;
    bool operator==(const GuiResourceLayoutOverride&) const = default;
};

struct GuiArchitectureWorkspace {
    GuiArchitectureMode mode{GuiArchitectureMode::Select};
    float pan_x{};
    float pan_y{};
    float zoom{1.0F};
    std::vector<GuiTaskLayoutOverride> tasks;
    std::vector<GuiResourceLayoutOverride> resources;
    bool operator==(const GuiArchitectureWorkspace&) const = default;
};

void normalize_architecture_workspace(GuiArchitectureWorkspace& state) noexcept;
void reset_architecture_layout(GuiArchitectureWorkspace& state) noexcept;
void auto_layout_architecture(GuiArchitectureWorkspace& state) noexcept;
void set_task_layout_position(GuiArchitectureWorkspace& state, TaskId task_id,
                              GuiLayoutPoint position);
void set_resource_layout_position(GuiArchitectureWorkspace& state, ResourceId resource_id,
                                  GuiLayoutPoint position);
void set_resource_layout_size(GuiArchitectureWorkspace& state, ResourceId resource_id,
                              GuiLayoutSize size);
void reset_task_layout_position(GuiArchitectureWorkspace& state, TaskId task_id);
void reset_resource_layout_position(GuiArchitectureWorkspace& state, ResourceId resource_id);
void reset_resource_layout_size(GuiArchitectureWorkspace& state, ResourceId resource_id);
const GuiTaskLayoutOverride* find_task_layout(const GuiArchitectureWorkspace& state,
                                               TaskId task_id) noexcept;
const GuiResourceLayoutOverride* find_resource_layout(const GuiArchitectureWorkspace& state,
                                                       ResourceId resource_id) noexcept;

} // namespace cpssim
