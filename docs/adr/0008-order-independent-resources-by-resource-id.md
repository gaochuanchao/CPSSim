# ADR-0008: Order Independent Resources by Resource ID

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T7, T9, T10, T12

## Context

T10 adds local and cloud resources that execute simultaneously but do not share
a ready queue. The engine needs one execution interval and expected completion
per resource. It also needs a deterministic rule when several resources start,
preempt, resume, or finish jobs at the same logical tick.

Configuration array order is stable input, but it is not a semantic resource
priority. Using it as a hidden tie breaker would allow two equivalent files
with reordered resource declarations to produce different canonical traces.

## Decision

### Independent runtime state

The T10 implementation was refined by ADR-0009. Runtime `Scheduler` owns one
state record per configured resource. Each record groups:

- one exclusive `Resource` with optional Running job, active interval,
  expected completion, and busy-time accounting; and
- the scheduler-owned Ready identities for that resource.

Jobs remain permanently associated with the resource captured from their
runtime task assignment. No global ready queue, migration, or cross-resource
preemption is introduced.

### Mapping

`ConfiguredResourceAllocator` stores an explicit vector of `TaskAssignment`
records supplied by an experiment runner. The engine validates complete task
coverage, uniqueness, configured resource IDs, and task-resource
accessibility before applying any assignment.

The allocator does not parse files. A later runner or configuration component
may read a user file and construct the assignment vector without adding JSON
or filesystem dependencies to the policy layer.

### Same-tick order

The engine preserves the global event-phase order from ADR-0004:

1. all valid completions;
2. all deadline checks;
3. all releases;
4. runtime scheduling; and
5. scheduling observations.

Before construction completes, runtime resource records are sorted by
ascending `ResourceId`. During step 4, the engine invokes its runtime
`Scheduler`, which consults the same injected `SchedulingPolicy` for resources
in that order. Scheduling events therefore receive insertion sequences in
ascending resource-ID order.

Completion candidates keep the insertion sequence assigned when each job
starts. Because simultaneous starts are generated in ascending `ResourceId`,
their simultaneous completions also have deterministic resource-ID order.

## Consequences

Positive:

- local and cloud jobs execute concurrently without sharing mutable state;
- a preemption on one resource cannot charge or interrupt another resource;
- equivalent resource declaration orders produce the same scheduling order;
- stale completion matching is isolated per resource; and
- the read-only `SchedulingPolicy` is reused without gaining mutation access.

Limiting:

- one policy implementation is used for every resource in a run;
- resources remain exclusive uniprocessors;
- assignment is fixed before initial releases;
- configured allocation records must currently be constructed by the caller;
- no migration or global optimization occurs at runtime; and
- processing remains single-threaded even though logical execution overlaps.

## Alternatives considered

### One global ready queue

Rejected because T10 models independent resources and explicitly excludes
global scheduling and migration.

### Use configuration array order

Rejected because declaration layout is not a scheduling semantic. Stable
`ResourceId` is already the canonical resource identity.

### Run one independent engine per resource

Rejected because releases, deadline phases, event sequences, and the canonical
trace require one deterministic global event timeline.

### Execute resources on operating-system threads

Rejected because logical concurrency does not require wall-clock parallelism.
Threads would add nondeterministic scheduling and synchronization without
improving the current research baseline.

## Validation

Integration tests configure resources in reverse ID order, verify simultaneous
starts and completions in ascending ID order, verify local preemption without
cloud interference, reject inaccessible and unknown mappings, and compare
multi-resource JSON traces byte-for-byte across repeated runs and reordered
resource declarations.
