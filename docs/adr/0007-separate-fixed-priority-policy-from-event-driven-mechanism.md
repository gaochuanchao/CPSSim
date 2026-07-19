# ADR-0007: Separate Fixed-Priority Policy from Event-Driven Mechanism

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T7, T8, T9, T10, T12

## Context

T9 must reproduce the MATLAB scheduler's release, start, completion,
preemption, resumption, and deadline behavior without returning to a
tick-by-tick table implementation. The architecture also requires scheduling
policy to remain separate from state-changing mechanism.

The MATLAB baseline defines:

- smaller numeric priority is higher;
- ready ordering is priority, release tick, task ID, then job ID;
- equal priority does not preempt;
- execution started at tick `t` occupies `[t, t + 1)` for its first tick;
- final work completed during `[t - 1, t)` finishes at tick `t`;
- completion at the absolute deadline is on time; and
- a task releasing a new job while its previous job is active is unsupported.

An event-driven engine also needs a completion strategy. Preemption can make a
previously scheduled completion obsolete, while T7 deliberately provides no
general event cancellation.

## Decision

### Original T9 interface and fixed-priority implementation

`Scheduler` is a small abstract base interface. It receives read-only
`Resource` and job-store views, returns the identity of one selected Ready job,
and reports whether that job should preempt the running job. It does not mutate
jobs, resources, events, or time.

`FixedPriorityScheduler` implements this interface. Runtime polymorphism is
used because alternative scheduler implementations are an expected experiment
dimension. The interface contains only the two decisions the engine already
needs; state transitions remain outside it.

Selection uses this complete key:

```text
(priority, release tick, TaskId, JobId)
```

Smaller values come first. Preemption occurs only when the selected Ready job
has a strictly smaller priority value than the running job.

### Mechanism and ownership

`SimulationEngine` owns, for one run:

- current tick;
- `EventQueue`;
- `PeriodicReleaseModel`;
- one runtime `Resource` in T9;
- the canonical `JobState` collection;
- execution-start and expected-completion ticks; and
- the append-only processed `Event` trace.

The engine stores a read-only reference to the injected `Scheduler` and applies
its decisions through public `Resource` transition methods. The scheduler must
outlive the engine and never receives mutable access.

Initial task placement is a different decision owned by `ResourceAllocator`
under ADR-0006. A scheduler orders released Ready jobs on their captured
resource; it does not assign tasks to resources.

### Same-tick cycle

At each event tick, the engine processes:

1. valid execution completion;
2. deadline check;
3. every periodic release;
4. one fixed-priority scheduling decision; and
5. the resulting preempt/start/resume observations.

This uses ADR-0004 phase precedence. All releases at a tick are Ready before
one scheduling decision. A completion exactly at its deadline is completed in
phase 1, so its phase-2 deadline candidate produces no miss.

### Event-driven execution accounting

Starting or resuming a job records `running_since` and schedules an expected
completion at:

```text
current tick + remaining execution
```

When preempted, the engine charges `preemption tick - running_since` once. The
old completion event remains in the queue but is ignored unless its job
identity and tick both match the resource's current expected completion. A
resume schedules a new expected completion. This is deterministic stale-event
invalidation, not general cancellation.

No completion beyond the inclusive run horizon is scheduled. Releases and
starts at the horizon are still observed, but no interval after it executes.

### Deadline and overlap behavior

Every released job receives one deadline-check candidate at its absolute
deadline when that tick is in horizon. `JobState` records whether the check
found it incomplete. A later completion retains the miss flag.

T9 rejects a release when the same task already has a Ready or Running job.
An explicit overrun/cancellation policy is required before this can change.

### T9 configuration boundary

`SimulationEngine` accepts exactly one resource. It receives a complete plan
from `ResourceAllocator`, validates exactly one assignment per task, and applies
the plan before release initialization. This original T9 boundary is
generalized by T10 in
[ADR-0008](0008-order-independent-resources-by-resource-id.md), without moving
placement back into `TaskSpec`.

### T10 ownership amendment

ADR-0009 refines names and ownership without changing the fixed-priority key,
strict-preemption rule, event phases, or interval accounting described here:

- the former read-only `Scheduler` interface is now `SchedulingPolicy`;
- `FixedPriorityScheduler` is now `FixedPriorityPolicy`;
- a kernel `Scheduler` owns jobs, per-resource Ready queues, resources, and
  state-changing scheduling coordination;
- each `Resource` owns its Running interval, completion expectation, and busy
  accounting; and
- `SimulationEngine` owns global time, queue, release orchestration, and trace.

The rest of this ADR describes the original T9 implementation and rationale.
Where an owner or current type name differs, ADR-0009 is authoritative.

## Consequences

Positive:

- schedulers can be unit-tested without executing state transitions;
- the same engine accepts different scheduler implementations;
- placement and runtime scheduling are separate extension points;
- execution jumps between events rather than iterating every tick;
- remaining execution is charged exactly once across preemption;
- same-tick releases lead to one deterministic decision;
- completion/deadline behavior matches the MATLAB contract; and
- pending candidate events remain separate from processed trace history.

Limiting:

- obsolete completion candidates remain pending until their tick;
- event sequence is insertion identity, not necessarily processed-trace order;
- the engine is one-resource-only until T10;
- self-overlap aborts instead of applying an overrun policy; and
- no network or caused-action event is processed in T9.

## Alternatives considered

### Put selection inside `Resource`

Rejected because it would combine scheduling policy with execution mechanism
and make alternative policies harder to test.

### Keep a concrete fixed-priority member in `SimulationEngine`

Rejected because upcoming experiments need different scheduling policies. A
two-method base interface is now stable enough to justify runtime polymorphism
without moving mechanism into derived schedulers.

### Execute one loop iteration for every tick

Rejected because CPSSim's kernel is event-driven and long idle spans should
not cost work proportional to physical duration.

### Add general queue cancellation now

Deferred because no other producer requires cancellation. Expected-completion
matching solves the concrete T9 need with a smaller interface.

### Preempt equal-priority jobs

Rejected because the MATLAB reference explicitly keeps the running job.

## Validation

Scheduler tests cover the complete selection key, strict preemption, and
inconsistent read-only views. Engine tests cover alternative scheduler
injection, allocation-plan validation, normal completion,
preemption/resumption accounting, equal-priority waiting, late and exact
deadline outcomes, self-overlap rejection, allocation-plan validation,
inclusive horizon behavior, and byte-repeatable processed JSON traces.

## T10 follow-up

T10 retains the same policy decisions and state-transition semantics. The
runtime scheduler visits independent resources in ascending `ResourceId`.
Per-resource ordering is recorded in
[ADR-0008](0008-order-independent-resources-by-resource-id.md); current
ownership and naming are recorded in
[ADR-0009](0009-separate-scheduler-policy-resource-and-engine-ownership.md).
