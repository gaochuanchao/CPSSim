# ADR-0001: Use Signed Integer Ticks as Canonical Time

- Status: Accepted
- Date: 2026-07-17

## Context

CPSSim must reproduce deterministic event timing across repeated runs,
compilers, build modes, and future GUI or adapter integrations. Floating-point
timestamps can make logically equal times compare differently after arithmetic
and can therefore change same-time event ordering or scheduler behavior.

The Bosch reference uses a 0.1 ms quantum and represents scheduler time with
integer tick indices. The portable C++ model needs to preserve this behavior
without hard-coding the core to one physical quantum.

## Decision

Canonical simulator and trace time is `Tick`, an alias of `std::int64_t`.
Valid canonical event timestamps are nonnegative.

The physical duration of one tick is supplied explicitly at a conversion
boundary as a positive integer number of nanoseconds. Initial conversion
helpers use `std::chrono::nanoseconds` and:

- reject nonpositive tick periods;
- reject negative durations and tick counts;
- require duration-to-tick conversion to be exact;
- reject overflow; and
- never silently round.

Floating-point seconds may later be accepted or produced by display, file, or
external-adapter boundaries, but they are not canonical timestamps. Such a
boundary must define and validate its rounding policy explicitly.

## Consequences

- Equality and ordering of canonical timestamps are exact.
- The Bosch quantum is represented exactly as 100,000 nanoseconds per tick.
- Same-time event logic will not depend on floating-point comparison.
- Invalid or inexact conversions fail early rather than shifting an event.
- A signed representation supports checked subtraction and makes invalid
  negative input detectable, while valid event timestamps remain nonnegative.
- The initial physical-duration boundary cannot represent sub-nanosecond tick
  periods. Supporting them would require a later, explicit rational-time
  decision that supersedes this ADR.

## Alternatives considered

### Floating-point seconds as canonical time

Rejected because arithmetic and equality would depend on rounding and could
affect deterministic event ordering.

### Implicit rounding to the nearest tick

Rejected because moving an event across a tick changes observable scheduling
semantics. Any future adapter that needs rounding must expose its policy.

### Unsigned canonical ticks

Rejected for the initial API because signed values make differences and
validation easier while still providing more than enough range for the target
experiments.
