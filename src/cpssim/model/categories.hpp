/***
 * File: src/cpssim/model/categories.hpp
 * Purpose: Define the finite event, lifecycle, and scheduling categories
 *          shared by the portable simulator model.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: These scoped enums define names only. Runtime transitions are
 *        enforced by Resource, while canonical event records begin in T6.
 ***/

#pragma once

namespace cpssim {

/*** Enumerates observable categories of canonical simulator events. ***/
enum class EventType {
    JobRelease,
    JobStart,
    JobPreempt,
    JobResume,
    JobFinish,
    DeadlineMiss,
    MessageSend,
    MessageDelivery,
};

/***
 * Identifies the semantic processing stage of an event. EventQueue maps every
 * value to explicit same-tick precedence without using enum numeric values.
 ***/
enum class EventPhase {
    ExecutionCompletion,
    MessageDelivery,
    DeadlineCheck,
    JobRelease,
    PolicyUpdate,
    Scheduling,
    CausedAction,
};

/*** Enumerates mutually exclusive lifecycle states of one job instance. ***/
enum class JobLifecycle {
    Ready,
    Running,
    Completed,
    Cancelled,
};

/*** Selects whether a scheduler may replace an incomplete Running job. ***/
enum class PreemptionMode {
    Preemptive,
    NonPreemptive,
};

/*** Enumerates the observable transmission state of one runtime message. ***/
enum class MessageLifecycle {
    PendingSend,
    InFlight,
    Delivered,
};

} // namespace cpssim
