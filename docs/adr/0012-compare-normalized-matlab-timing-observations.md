# ADR-0012: Compare Normalized MATLAB Timing Observations

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T1, T6, T10, T11, T12, T13

## Context

T12 must decide whether CPSSim reproduces the dedicated-resource and
shared-cloud MATLAB timing references. The two implementations do not use the
same internal records:

- MATLAB assigns a global row number to its precomputed job and packet tables;
- CPSSim creates jobs and messages incrementally at runtime;
- CPSSim's `EventSequence` identifies every scheduled candidate, including
  internal deadline and stale-completion candidates that are absent from the
  MATLAB scheduler-event table; and
- MATLAB network rows contain Bosch direction and trigger-column fields that
  the generic simulator core deliberately does not know.

Comparing those implementation-specific identities would report false
divergences even when the observable timing behavior is equal. Comparing only
aggregate counts would hide the location and nature of a real divergence.

## Decision

T12 adds a Bosch-reference conformance component and executable outside
`cpssim_core`. It reads the captured CSV files, constructs the corresponding
generic CPSSim experiment, runs the normal `SimulationEngine`, projects both
results into comparable observations, and reports the first unequal row.

### Scheduler projection

Scheduler observations are compared in processed order using:

1. integer event tick;
2. lifecycle event type (`release`, `start`, `preempt`, `resume`, `finish`, or
   `deadline_miss`);
3. task ID and task name;
4. task-local job ID; and
5. resource name.

The floating-point `eventTimeSec`, MATLAB `sequence`, and MATLAB `eventJobRow`
are excluded. Integer ticks are canonical time. MATLAB sequence is its output
row, whereas CPSSim sequence is pending-event insertion identity.

### Network projection

Network observations are compared in publication order using:

1. source task ID/name and task-local source job ID;
2. configured destination task ID/name;
3. publication, planned send, and planned delivery ticks; and
4. whether send and delivery occur within the inclusive trace horizon.

MATLAB `packetId` is excluded because it belongs to the offline packet table;
CPSSim `MessageId` follows deterministic online creation order as accepted by
[ADR-0010](0010-generate-task-routed-messages-with-fixed-delay.md). Bosch
direction names and trigger columns are also excluded because they belong to
the T13 adapter. The source job and destination retain the causal meaning.

### Failure reporting

The comparison stops at the first unequal observation and reports:

- the stream (`scheduler_events` or `network_events`);
- the one-based comparable row;
- the expected observation;
- the actual observation, or `<missing>`;
- and the preceding matched row, or `none`.

Counts are always reported for both streams. Exact equality means every field
and every row in both normalized streams matches; it does not mean that the
implementation-specific CSV bytes or internal IDs are equal.

The first T12 run exposed different ordering for runtime-generated releases
at one tick. The accepted correction is recorded separately in
[ADR-0013](0013-order-same-tick-periodic-releases-by-task-semantics.md).

## Consequences

Positive:

- the two required scenarios can run as automated CTest conformance tests;
- a failure points to the first causal timing difference rather than only a
  final count;
- CPSSim remains dynamically event-driven and does not adopt MATLAB's offline
  tables;
- Bosch CSV vocabulary stays outside the generic core; and
- T13 can later add a separate trigger projection without changing T12 timing
  semantics.

Limiting:

- the T12 reader supports the pinned reference CSV schema, not arbitrary CSV;
- task and resource names are part of this reference comparison even though
  stable numeric IDs drive the core;
- numerical FMU outputs and trigger columns remain untested until T13/T14;
- packet payload and channel semantics are not compared; and
- changing a golden file still requires the documented checksum and review
  process.

## Validation

- dedicated-resource timing must match all scheduler and network observations;
- shared-cloud timing must match all scheduler and network observations;
- a deliberately altered fixture must produce a first-row divergence with
  expected and actual records;
- both scenario commands must be registered directly with CTest; and
- Debug, Release, sanitizer, Clang, and clang-tidy builds must pass.
