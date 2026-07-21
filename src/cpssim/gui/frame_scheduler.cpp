/*** Implement graphics-independent GUI frame scheduling policy. ***/

#include "cpssim/gui/frame_scheduler.hpp"

#include <algorithm>

namespace cpssim {

GuiFrameActivity classify_gui_frame_activity(const GuiFrameActivityInput& input) noexcept {
    if (input.run_state == GuiRunState::Running || input.queued_work) {
        return GuiFrameActivity::Running;
    }
    if (input.interactive) {
        return GuiFrameActivity::Interactive;
    }
    if (input.background_pending) {
        return GuiFrameActivity::BackgroundPending;
    }
    return GuiFrameActivity::FullyIdle;
}

GuiWaitStrategy gui_wait_strategy(GuiFrameActivity activity) noexcept {
    switch (activity) {
    case GuiFrameActivity::Running:
    case GuiFrameActivity::Interactive:
        return GuiWaitStrategy::Poll;
    case GuiFrameActivity::BackgroundPending:
        return GuiWaitStrategy::WaitWithTimeout;
    case GuiFrameActivity::FullyIdle:
        return GuiWaitStrategy::WaitIndefinitely;
    }
    return GuiWaitStrategy::WaitIndefinitely;
}

bool GuiPointerRect::contains(GuiPointerPoint point) const noexcept {
    return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

void GuiPointerRegionMap::begin_frame() { building_.clear(); }

void GuiPointerRegionMap::add(GuiPointerRegion region) {
    if (region.bounds.right >= region.bounds.left && region.bounds.bottom >= region.bounds.top) {
        building_.push_back(region);
    }
}

void GuiPointerRegionMap::publish() {
    published_ = building_;
    valid_ = true;
}

void GuiPointerRegionMap::invalidate() noexcept { valid_ = false; }

std::optional<GuiPointerRegion>
GuiPointerRegionMap::hit_test(GuiPointerPoint point) const noexcept {
    if (!valid_) {
        return std::nullopt;
    }
    const auto found =
        std::find_if(published_.rbegin(), published_.rend(),
                     [point](const auto& region) { return region.bounds.contains(point); });
    return found == published_.rend() ? std::nullopt : std::optional{*found};
}

bool GuiPointerRedrawPolicy::cursor_moved(GuiPointerPoint point,
                                          const GuiPointerRegionMap& regions) {
    if (!regions.valid()) {
        last_position_ = point;
        hovered_identity_.reset();
        hovered_behavior_.reset();
        return true;
    }
    const auto hit = regions.hit_test(point);
    const auto previous = hovered_identity_;
    const auto previous_behavior = hovered_behavior_;
    hovered_identity_ = hit ? std::optional{hit->identity} : std::nullopt;
    hovered_behavior_ = hit ? std::optional{hit->behavior} : std::nullopt;
    const auto position_changed =
        !last_position_.has_value() || last_position_->x != point.x || last_position_->y != point.y;
    last_position_ = point;
    if (!hit.has_value()) {
        return previous_behavior == GuiPointerRegionBehavior::BoundarySensitive ||
               previous_behavior == GuiPointerRegionBehavior::PositionSensitive;
    }
    switch (hit->behavior) {
    case GuiPointerRegionBehavior::Passive:
        return previous_behavior == GuiPointerRegionBehavior::BoundarySensitive &&
               previous != hovered_identity_;
    case GuiPointerRegionBehavior::BoundarySensitive:
        return previous != hovered_identity_;
    case GuiPointerRegionBehavior::PositionSensitive:
        return position_changed;
    case GuiPointerRegionBehavior::DragHandle:
        return button_pressed_ && position_changed;
    }
    return false;
}

} // namespace cpssim
