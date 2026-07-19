/***
 * File: tests/model/categories_test.cpp
 * Purpose: Verify that event, lifecycle, and preemption categories are scoped
 *          enum types with distinct values.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/model/categories.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace {

using cpssim::EventPhase;
using cpssim::EventType;
using cpssim::JobLifecycle;
using cpssim::MessageLifecycle;
using cpssim::PreemptionMode;

static_assert(std::is_enum_v<EventType>);
static_assert(std::is_enum_v<EventPhase>);
static_assert(std::is_enum_v<JobLifecycle>);
static_assert(std::is_enum_v<MessageLifecycle>);
static_assert(std::is_enum_v<PreemptionMode>);
static_assert(!std::is_convertible_v<EventType, int>);
static_assert(!std::is_convertible_v<EventPhase, int>);
static_assert(!std::is_convertible_v<JobLifecycle, int>);
static_assert(!std::is_convertible_v<MessageLifecycle, int>);
static_assert(!std::is_convertible_v<PreemptionMode, int>);

/*** Verifies that event categories represent distinguishable observations. ***/
TEST_CASE("event categories distinguish lifecycle observations", "[model][event]") {
    STATIC_REQUIRE(EventType::JobRelease != EventType::JobStart);
    STATIC_REQUIRE(EventType::JobFinish != EventType::DeadlineMiss);
    STATIC_REQUIRE(EventType::MessageSend != EventType::MessageDelivery);
    STATIC_REQUIRE(EventPhase::ExecutionCompletion != EventPhase::Scheduling);
}

/*** Verifies the ordered conceptual stages of a fixed-delay message. ***/
TEST_CASE("message lifecycle categories are scoped and distinct", "[model][lifecycle][network]") {
    STATIC_REQUIRE(MessageLifecycle::PendingSend != MessageLifecycle::InFlight);
    STATIC_REQUIRE(MessageLifecycle::InFlight != MessageLifecycle::Delivered);
}

/*** Verifies that active and terminal lifecycle categories remain distinct. ***/
TEST_CASE("job lifecycle categories are scoped and distinct", "[model][lifecycle]") {
    STATIC_REQUIRE(JobLifecycle::Ready != JobLifecycle::Running);
    STATIC_REQUIRE(JobLifecycle::Running != JobLifecycle::Completed);
    STATIC_REQUIRE(JobLifecycle::Completed != JobLifecycle::Cancelled);
}

/*** Verifies that the two scheduler preemption behaviors remain distinct. ***/
TEST_CASE("preemption modes are scoped and distinct", "[model][scheduling]") {
    STATIC_REQUIRE(PreemptionMode::Preemptive != PreemptionMode::NonPreemptive);
}

} // namespace
