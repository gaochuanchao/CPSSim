/*** Pure activity, wait, redraw, and high-value pointer-region policy. ***/

#pragma once

#include "cpssim/gui/simulation_controller.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cpssim {

enum class GuiFrameActivity { Running, Interactive, BackgroundPending, FullyIdle };
enum class GuiWaitStrategy { Poll, WaitWithTimeout, WaitIndefinitely };
enum class GuiPointerRegionBehavior { Passive, BoundarySensitive, PositionSensitive, DragHandle };

struct GuiFrameActivityInput {
    GuiRunState run_state{GuiRunState::NotConfigured};
    bool queued_work{false};
    bool interactive{false};
    bool background_pending{false};
};

GuiFrameActivity classify_gui_frame_activity(const GuiFrameActivityInput& input) noexcept;
GuiWaitStrategy gui_wait_strategy(GuiFrameActivity activity) noexcept;

struct GuiPointerPoint {
    float x{};
    float y{};
};

struct GuiPointerRect {
    float left{};
    float top{};
    float right{};
    float bottom{};

    bool contains(GuiPointerPoint point) const noexcept;
};

struct GuiPointerRegion {
    std::uint64_t identity{};
    GuiPointerRect bounds;
    GuiPointerRegionBehavior behavior{GuiPointerRegionBehavior::Passive};
};

class GuiPointerRegionMap {
  public:
    void begin_frame();
    void add(GuiPointerRegion region);
    void publish();
    void invalidate() noexcept;
    bool valid() const noexcept { return valid_; }
    std::optional<GuiPointerRegion> hit_test(GuiPointerPoint point) const noexcept;

  private:
    std::vector<GuiPointerRegion> building_;
    std::vector<GuiPointerRegion> published_;
    bool valid_{false};
};

class GuiPointerRedrawPolicy {
  public:
    bool cursor_moved(GuiPointerPoint point, const GuiPointerRegionMap& regions);
    void button_changed(bool pressed) noexcept { button_pressed_ = pressed; }
    void clear_hover() noexcept { hovered_identity_.reset(); }

  private:
    std::optional<std::uint64_t> hovered_identity_;
    std::optional<GuiPointerPoint> last_position_;
    bool button_pressed_{false};
};

class GuiRedrawTracker {
  public:
    void request() noexcept { ++requested_generation_; }
    bool pending() const noexcept { return rendered_generation_ != requested_generation_; }
    void acknowledge() noexcept { rendered_generation_ = requested_generation_; }
    std::uint64_t generation() const noexcept { return requested_generation_; }

  private:
    std::uint64_t requested_generation_{1};
    std::uint64_t rendered_generation_{0};
};

} // namespace cpssim
