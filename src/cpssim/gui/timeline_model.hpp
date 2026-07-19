/***
 * File: src/cpssim/gui/timeline_model.hpp
 * Purpose: Declare graphics-independent G05 scheduling timeline records.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Canonical ticks remain integers; open intervals have no synthetic end.
 ***/

#pragma once

#include "cpssim/gui/presentation_model.hpp"
#include "cpssim/model/event.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

enum class GuiTimelineIntervalKind {
    Ready,
    Running,
};

/*** Represents one exact half-open lifecycle interval or an open live interval. ***/
struct GuiTimelineInterval {
    GuiTimelineIntervalKind kind;
    JobIdentity job;
    ResourceId resource_id;
    Tick begin_tick;
    std::optional<Tick> end_tick;
    EventSequence begin_sequence;
    std::optional<EventSequence> end_sequence;
    bool resumed;

    bool operator==(const GuiTimelineInterval&) const = default;
};

/*** Copies one canonical observation for point-marker rendering and selection. ***/
struct GuiTimelineMarker {
    Tick tick;
    EventSequence sequence;
    EventType type;
    std::optional<TaskId> task_id;
    std::optional<JobId> job_id;
    std::optional<ResourceId> resource_id;
    std::optional<MessageId> message_id;
    std::optional<EventSequence> cause_sequence;

    bool operator==(const GuiTimelineMarker&) const = default;
};

/*** Owns all derived lifecycle intervals for one configured resource. ***/
struct GuiTimelineRow {
    ResourceId resource_id;
    std::string label;
    std::vector<GuiTimelineInterval> intervals;

    bool operator==(const GuiTimelineRow&) const = default;
};

/*** Maps a stable event identity to its marker without assuming sequence order. ***/
struct GuiTimelineMarkerIndexEntry {
    EventSequence sequence;
    std::size_t marker_index;

    bool operator==(const GuiTimelineMarkerIndexEntry&) const = default;
};

/*** Owns a complete detached derivation at one snapshot tick. ***/
struct GuiTimelineModel {
    Tick current_tick;
    std::vector<GuiTimelineRow> rows;
    std::vector<GuiTimelineMarker> markers;
    std::vector<GuiTimelineMarkerIndexEntry> marker_index;

    bool operator==(const GuiTimelineModel&) const = default;
};

enum class GuiTimelineDiagnosticCode {
    InvalidCurrentTick,
    InvalidTraceOrder,
    DuplicateEventSequence,
    UnobservedCause,
    WrongEventPhase,
    MissingEntity,
    UnknownEntity,
    InvalidJobTransition,
    ResourceMismatch,
    OverlappingRunningIntervals,
    InvalidInterval,
};

/*** Locates the first invalid canonical input without silently repairing it. ***/
struct GuiTimelineDiagnostic {
    GuiTimelineDiagnosticCode code;
    std::size_t event_index;
    std::optional<EventSequence> event_sequence;
    std::optional<Tick> tick;
    std::string message;

    bool operator==(const GuiTimelineDiagnostic&) const = default;
};

/*** Contains either one complete model or exactly one located diagnostic. ***/
struct GuiTimelineBuildResult {
    std::optional<GuiTimelineModel> timeline;
    std::vector<GuiTimelineDiagnostic> diagnostics;

    bool valid() const { return timeline.has_value() && diagnostics.empty(); }
};

/*** Strictly derives timeline presentation without mutating or adding events. ***/
GuiTimelineBuildResult build_timeline_model(const std::vector<Event>& events,
                                            const ExperimentPresentationSnapshot& experiment,
                                            Tick current_tick);

/***
 * Retains validated lifecycle state across append-only controller snapshots.
 * Reset, configuration replacement, or a changed trace boundary causes an
 * automatic full rebuild; an unchanged trace only updates the display tick.
 ***/
class GuiTimelineCache {
  public:
    GuiTimelineCache();
    ~GuiTimelineCache();
    GuiTimelineCache(GuiTimelineCache&&) noexcept;
    GuiTimelineCache& operator=(GuiTimelineCache&&) noexcept;
    GuiTimelineCache(const GuiTimelineCache&) = delete;
    GuiTimelineCache& operator=(const GuiTimelineCache&) = delete;

    const GuiTimelineBuildResult& update(const std::vector<Event>& events,
                                         const ExperimentPresentationSnapshot& experiment,
                                         Tick current_tick);
    void clear();

  private:
    struct State;
    std::unique_ptr<State> state_;
};

/*** Finds the marker for one stable canonical event sequence in logarithmic time. ***/
const GuiTimelineMarker* find_timeline_marker(const GuiTimelineModel& timeline,
                                              EventSequence sequence);

} // namespace cpssim
