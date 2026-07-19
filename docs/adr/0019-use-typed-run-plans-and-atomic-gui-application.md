# ADR-0019: Use Typed Run Plans and Atomic GUI Application

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: G03, F1

## Context

The production GUI currently constructs a demonstration allocation by choosing
the first declared task-resource profile for each task. That choice is
deterministic, but it is implicit application behavior rather than an explicit
research-experiment input. The future F1 command-line runner also needs a
validated task allocation, stop tick, and scheduling-policy selection without
putting those choices into `TaskSpec` or teaching an allocator to parse files.

G03 needs editable values before a simulation exists and must preserve an
existing run when a draft is incomplete or invalid. It must not mutate live
runtime tasks, migrate jobs, or let presentation timing affect simulation
semantics. The experiment/run-plan ownership boundary, validation location,
controller replacement, and future persistence format therefore require an
architecture decision before implementation.

## Proposed decision

### Experiment configuration and run-plan ownership

`ExperimentConfig` remains the validated immutable description of what may be
simulated. It continues to own:

- tick duration;
- task timing and priority;
- resource specifications;
- accessible task-resource profiles and their execution demands;
- message routes; and
- experiment-wide preemption mode, as required by ADR-0009.

A new immutable `RunPlan` value in the shared, non-GUI core boundary describes
how one run of that experiment is constructed. It contains:

- exactly one `TaskAssignment` for every configured task;
- a nonnegative inclusive stop tick; and
- a `SchedulingPolicyKind`, initially supporting only fixed priority.

Assignments are stored in configured task order after successful validation.
This canonical order avoids making input order or hash-container iteration part
of run identity.

The first run-plan version does not contain policy parameters because the
fixed-priority policy has none. It does not contain a random seed because no
current run-plan-selected stochastic source exists. A seed must be added before
such a source is supported. Recording paths, export choices, and report options
belong to runner invocation/output-manifest data rather than simulation input
and are not part of `RunPlan`.

### Draft and shared validation boundary

`RunPlanDraft` is GUI-support/application data with optional editable fields.
It holds no pointer or mutable reference into `ExperimentConfig`, `RunPlan`,
`SimulationController`, or `SimulationEngine`. A new GUI draft starts with the
requested stop tick and policy kind but with no inferred task assignments; the
user must select every assignment explicitly.

The shared non-GUI run-plan builder validates a raw plan request against one
`ExperimentConfig` before engine construction. It returns either an immutable
`RunPlan` or typed, field-addressable diagnostics. Validation rejects:

- missing or duplicate task assignments;
- unknown task or resource IDs;
- task-resource pairs without an execution profile;
- negative stop ticks; and
- unsupported scheduling-policy kinds.

The builder is the user-input validation boundary shared by the GUI and future
F1 CLI. `SimulationEngine` retains its existing allocation validation as a
defensive public-interface invariant; the GUI does not rely on an exception
from a partly constructed runtime as field-level validation.

### Active plan, draft plan, and controller lifecycle

The GUI session owns the loaded immutable experiment, editable draft, accepted
active plan, and active controller. Before the first successful Apply, there is
no active simulation controller. Experiment and draft presentation therefore
must not depend on reading an engine snapshot.

Apply follows an atomic replacement sequence:

1. validate and build an immutable `RunPlan`;
2. construct a complete replacement `SimulationController` from copied
   `ExperimentConfig` and `RunPlan` values; and
3. replace the active plan and controller only after both steps succeed.

Any diagnostic or construction failure leaves the existing active plan,
controller, trace, and run state unchanged. A newly applied plan always starts
in the clean Paused state. `SimulationController::Reset` reconstructs policy,
allocator, and engine from its stored active `RunPlan`; it never reads the
current draft.

Semantic editing is disabled while the active controller is Running. While it
is Paused or Finished, the user may edit a separate pending draft. Those edits
do not affect the active run until explicit Apply, which performs the same clean
replacement sequence. The UI displays active and draft values distinctly.

Dirty state is derived by typed value comparison between the draft and active
plan fields, not maintained as an independent mutable flag. A draft without an
active plan is dirty. Editing a value back to the active value clears the dirty
state.

### Persistence and CLI staging

G03a through G03d add no run-plan file format. G03e remains a separate approval
and review slice. Consequently:

- experiment JSON remains schema version 4;
- assignments and stop tick are not added to `ExperimentConfig` JSON;
- no run-plan schema version is introduced by the in-memory implementation;
  and
- no experiment checksum, ID, or path association is invented prematurely.

If persistence is approved, its format must have its own schema version and a
defined experiment-association rule, deterministic serialization, strong IDs,
and actionable mismatch diagnostics. That decision may amend this ADR or add a
new one before G03e.

The shared typed `RunPlan`, raw request, builder, and diagnostics are usable by
both the GUI and CLI. G03 changes only the GUI call path. F1 may later add CLI
argument/file loading that produces the same raw request and consumes the same
builder without duplicating allocation validation or GUI code.

## Consequences

Positive:

- production GUI placement becomes explicit and inspectable;
- invalid drafts cannot partly replace a valid active run;
- Reset is reproducible from immutable accepted input;
- CLI and GUI share run semantics and validation without sharing presentation;
- experiment capabilities remain separate from per-run choices; and
- runtime assignment and job migration semantics do not change.

Limiting:

- the initial editor cannot save or load plans;
- a user must select every assignment before the first simulation exists;
- only fixed-priority policy construction is supported;
- editing is unavailable while Running; and
- future stochastic inputs and persistence require explicit follow-up design.

## Alternatives considered

### Keep choosing the first accessible profile

Rejected because an implicit application default is not a preserved research
experiment input and can change when profile ordering changes.

### Put selected assignments and stop tick in `ExperimentConfig`

Rejected because the configuration describes task-resource capabilities and
experiment-wide assumptions, while allocation and horizon select one run.
Combining them would reverse the separation established by ADR-0006 and
ADR-0002.

### Let `RunPlanDraft` directly mutate an active controller

Rejected because partial validation failure could leave mixed old and new
state, and live assignment changes would introduce reassignment or migration
semantics outside G03.

### Store an editable JSON document as the draft

Rejected for G03a through G03d because typed records give field-level
validation, strong identifiers, and refactoring safety without coupling GUI
widgets to a persistence representation.

### Add run-plan JSON in the same slice

Deferred because persistence needs independent decisions about schema
evolution, experiment association, paths, and reproducibility manifests. It is
not required to remove the production GUI's implicit assignment rule.

## Approval gate

No G03 C++ or GUI implementation begins while this ADR remains Proposed. After
the ownership, field scope, validation, lifecycle, dirty-state, CLI staging,
and persistence decisions above are approved, change the status to Accepted
and implement the approved G03 slices without including G03e persistence.
