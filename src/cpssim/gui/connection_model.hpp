/*** GUI-only logical and communication connection presentation identities. ***/
#pragma once

#include "cpssim/gui/presentation_model.hpp"

#include <compare>
#include <string>

namespace cpssim {

enum class GuiConnectionKind { Logical, Communication };

struct GuiConnectionId {
    GuiConnectionKind kind{GuiConnectionKind::Logical};
    TaskId source_task_id;
    TaskId destination_task_id;
    auto operator<=>(const GuiConnectionId&) const = default;
};

struct GuiConnectionPresentation {
    GuiConnectionId id;
    std::string label;
    Tick displayed_latency{};
    bool creates_network_events{false};
    bool protected_semantics{false};
    bool operator==(const GuiConnectionPresentation&) const = default;
};

struct GuiFunctionalDependency {
    TaskId source_task_id;
    TaskId destination_task_id;
    std::string label;
    bool operator==(const GuiFunctionalDependency&) const = default;
};

} // namespace cpssim
