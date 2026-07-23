# ADR-0009: Separate Scheduler, Policy, Resource, and Engine Ownership

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T4, T5, T9, T10, T11, T12

## Context

The T9/T10 implementation kept scheduling policy read-only, but placed the job
store, Ready membership, execution markers, completion handling, and dispatch
mechanism in `SimulationEngine`. Runtime `Resource` also owned Ready membership
while changing job execution state. This made the engine responsible for both
global discrete-event orchestration and most server-side scheduling behavior.

CPSSim also needs preemptive and non-preemptive experiments. Resource
allocation and future schedulability analysis must use the same preemption
assumption as runtime scheduling, while the engine must record that assumption
without implementing the scheduling decision itself.

## Decision

### Shared scheduling specification

`ExperimentConfig` owns one immutable `SchedulingSpec`. Its current field is a
`PreemptionMode` with values `Preemptive` and `NonPreemptive`. This is the one
source of truth available to the allocator, runtime scheduler, engine, and
experiment reporting.

JSON schema version 3 requires an explicit scheduling object. Versions 1 and 2
remain readable and translate to `Preemptive`, which preserves their historical
fixed-priority baseline.

T11's schema version 4 retains this required scheduling object and adds
message routes; it does not change the ownership decision here.

### Scheduling policy

`SchedulingPolicy` is the read-only runtime-polymorphic decision interface. It
selects from a supplied Ready view and reports whether the selected job outranks
the Running job. `FixedPriorityPolicy` implements the deterministic
`(priority, release tick, TaskId, JobId)` order.

The policy owns no runtime state and does not mutate jobs, resources, events,
or time.

### Runtime scheduler

One runtime `Scheduler` owns the T10 scheduling domain:

- the canonical `JobState` collection;
- one Ready queue per managed resource;
- the managed runtime `Resource` values;
- the configured preemption mode; and
- coordination of submission, deadline handling, completion, dispatch, and
  preemption.

The scheduler uses an injected `SchedulingPolicy`. In non-preemptive mode it
does not replace a Running job. In preemptive mode it consults the policy
before commanding a resource to stop its current job.

Resources and their Ready queues are ordered by ascending `ResourceId` so the
T10 deterministic scheduling order is preserved.

### Runtime resource

`Resource` represents one exclusive execution unit. It owns:

- the optional Running job identity;
- the current execution-interval start;
- the expected completion tick; and
- accumulated integer busy ticks.

It starts, charges, preempts, and completes the job selected by the scheduler.
It does not own Ready membership and never chooses a job or scheduling policy.
Busy and idle observations remain integer tick counts over `[0, observation)`;
floating-point utilization is a reporting concern.

### Simulation engine

`SimulationEngine` owns global orchestration:

- current logical tick and stop tick;
- the global deterministic `EventQueue`;
- `PeriodicReleaseModel` progression;
- allocation-plan validation and application;
- routing processed events to the runtime scheduler; and
- the append-only canonical trace.

The engine owns one runtime scheduler for the current T10 scope and delegates
job/resource inspection to it. It knows the immutable scheduling specification
but does not decide whether a Running job is preempted.

### Current server boundary

T10 continues to use one scheduler over all configured independent resources.
No `ServerId` or network endpoint is introduced by this refinement. Before
server-to-server communication is modeled, a later decision must define
whether local and cloud resources are separate servers or resources within one
scheduling domain.

## Consequences

Positive:

- the engine can focus on deterministic time, events, trace, and orchestration;
- scheduler mechanism and scheduling policy remain replaceable separately;
- resources own execution accounting without owning policy or Ready queues;
- allocation analysis and runtime scheduling share one preemption assumption;
- preemptive and non-preemptive runs can use the same task specifications and
  fixed-priority ordering; and
- integer resource accounting supports later utilization reporting.

Limiting:

- one scheduler and one scheduling specification cover all current resources;
- job ownership moves to the scheduler, so earlier T9/T10 public inspection
  calls change;
- JSON schema version 3 or later is required for explicit preemption
  configuration;
- legacy schemas imply the historical preemptive mode; and
- per-task non-preemptive sections, preemption costs, server identity, global
  scheduling, and migration remain outside the current scope.

## Alternatives considered

### Keep mechanism in `SimulationEngine`

Rejected because it mixes global orchestration with server-side Ready queues,
dispatch, execution accounting, and policy application.

### Let `Resource` own its Ready queue and policy

Rejected because a CPU-like resource executes work but does not choose the
system scheduling rule. This would also duplicate policy state across
resources.

### Put preemption mode in every task or job

Rejected for the baseline because preemptive versus non-preemptive fixed
priority is an experiment-wide scheduler behavior. A later task constraint may
represent non-preemptive critical sections without changing this global mode.

### Configure allocator and scheduler independently

Rejected because schedulability analysis could assume one mode while runtime
execution applies another.

## Validation

Tests must cover explicit and legacy scheduling configuration, preemptive and
non-preemptive runtime traces, policy replacement, Ready-queue ownership,
resource execution accounting, multi-resource isolation and ordering, and
byte-repeatable canonical traces.
