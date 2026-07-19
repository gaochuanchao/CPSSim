# Coding-Agent Handoff

This is the compact entry point for a coding agent. It reduces repeated context
loading; it does not override [AGENTS.md](../../AGENTS.md).

## Project now

CPSSim is a C++20 deterministic event-driven CPS research simulator. The
current implementation includes periodic task releases,
task placement, fixed-priority scheduling across independent exclusive
resources, causal fixed-delay messages, captured Bosch conformance, FMI 2.0
Co-Simulation import, online functional interaction, strict execution of all
three supplied Bosch example trajectory formats, and an optional panelized GUI
workbench with DPI-aware startup sizing, adjustable text, queued controls,
an Experiment Explorer, shared strong-ID selection, read-only inspection,
resource utilization, canonical events, and an explicit validated run-plan
editor with atomic Apply/Reset behavior and strict versioned JSON Load/Save.
The GUI also includes a deterministic pan/zoom architecture graph for tasks,
resources, causal message routes, applied assignments, shared selection, and
pending-draft task reassignment.
The GUI also includes a cached live scheduling timeline with Ready/Running
resource lanes, all canonical markers, exact malformed-trace locations,
pan/zoom/fit, current-tick cursor, entity/time filtering, and shared
cross-view selection. Incremental append results are checked against complete
headless rebuilds.
Functional GUI support adds detached typed observations, reset-safe
functional-model factories, discoverable scalar registries, incremental series
extraction, extrema-preserving visual downsampling, and a custom live
Functional signals plot synchronized with the scheduling timeline's time
selection. `cpssim_gui` accepts
`--mock-functional` as a dependency-free demonstration source.

Before selecting new work, verify the current roadmap and future-work guide
because this paragraph can become stale.

The reader-facing GUI documentation is intentionally limited to the
[GUI tutorial](../gui/README.md) and
[implemented GUI architecture](../gui/GUI_ARCHITECTURE.md). Completed GUI task
plans were removed after their durable behavior, ownership, tests, and future
gaps were integrated into current guides, ADRs, and the F3 backlog.

## Read only what the task needs

```text
Always: AGENTS.md + this page
Then:   relevant module page + relevant ADR + public header + closest test
Only if scope requires it: charter / architecture / roadmap / workflow policy
```

Useful entry points:

- system map: [Project tour](PROJECT-TOUR.md);
- exact behavioral rules: [Simulation semantics](SIMULATION-SEMANTICS.md);
- implementation/testing: [Developer guide](DEVELOPER-GUIDE.md);
- unimplemented proposals: [Future improvements](FUTURE-WORK.md);
- commands: [COMMANDS.md](../COMMANDS.md);
- proposed capabilities: [Future improvements](FUTURE-WORK.md).

## Invariants to preserve

- canonical event timestamps are integer ticks;
- queue order is `(tick, explicit phase precedence, insertion sequence)`;
- accepted trace is append-only and excludes stale candidates;
- immutable specifications and mutable runtime state stay separate;
- placement policy, scheduling policy, scheduling mechanism, resource
  execution, and global orchestration stay separate;
- core code does not depend on Bosch, FMI, MATLAB/Simulink, or Dear ImGui;
- no global mutable state and no bypass of public mutation interfaces;
- GUI wall-clock timing cannot affect simulation behavior;
- randomness requires explicit seeds and logged samples.

## Behavioral summary

```text
At tick t:
  settle work performed during the preceding interval
  -> accept deliveries
  -> check deadlines
  -> release jobs
  -> update policy
  -> schedule work for [t, t+1)
  -> record caused actions
```

A job that starts at `t` has received one tick of work at boundary `t+1`.
Completion exactly at a deadline is on time. Releases are generated at runtime,
one pending release per task. Fixed priority uses smaller values first; equal
priority does not preempt. Events at the inclusive stop tick are processed.
Read [Simulation semantics](SIMULATION-SEMANTICS.md) before changing any of
these rules.

## Ownership shorthand

| Owner | State/decision |
|---|---|
| `ExperimentConfig` | validated immutable input |
| `RunPlan` | validated assignments, inclusive stop tick, and scheduling-policy kind |
| `RunPlanDraft` | incomplete GUI/application choices, detached from active runtime |
| runtime `Task` | applied assignment and next release |
| `EventQueue` | pending candidates and sequence allocation |
| `SchedulingPolicy` | read-only ranking/preemption recommendation |
| `Scheduler` | jobs, Ready queues, transitions |
| `Resource` | active interval and busy-time accounting |
| `FixedDelayNetwork` | messages and lifecycle |
| `SimulationEngine` | current event tick, routing, accepted trace |
| `FunctionalRuntime` | functional lifecycle and observation trace |
| `BoschFmi2FunctionalModel` | immutable Bosch trajectory copy and FMI component lifecycle |
| `SimulationController` | GUI commands, current GUI-owned functional model, and detached snapshots |
| `GuiSimulationSession` | loaded experiment, functional-model factory/registry, draft validation, and atomic active-controller replacement |
| `ExperimentPresentationSnapshot` | sorted detached configuration and applied assignment copy |
| `GuiSelection` | selected experiment/task/resource/route/job/event identity and optional tick range |
| `GuiTimelineCache` | disposable validated timeline prefix and derived lifecycle presentation |
| `GuiSignalCache` | disposable validated functional schema and full-resolution derived scalar series |
| `GuiApplication` | panel visibility, view state, text scale, fixed workbench layout, and About-dialog state |

## Task protocol

1. Report intended change, state owner, files, validation, documentation, and
   ambiguities before editing.
2. Preserve unrelated user changes and inspect `git status` first.
3. Work in one small requested scope; do not begin the next roadmap target.
4. Add/update tests, implementation, and current documentation together.
5. Run focused tests plus the risk-appropriate completion commands.
6. Show the proposed diff; do not commit unless explicitly requested.

Common completion commands:

```bash
make test
make release
make asan
make format-check
git diff --check
```

For documentation-only changes, validate local links and run `make test`; do
not reformat source unnecessarily.

## Decisions that require an ADR

Create or amend an ADR before changing public architecture, state ownership,
time semantics, same-tick ordering, determinism, configuration/trace schema,
threading, or cross-platform behavior. When evidence conflicts with the Bosch
oracle, document the discrepancy and ask for a decision rather than simplifying
the C++ behavior silently.

## Known future seams, not current behavior

- the categorized backlog and staged implementation proposals are maintained
  in [Future improvements](FUTURE-WORK.md); its F-labels are not approved tasks;
- one directed channel and task port per configured task connection is planned
  in [ADR-0011](../adr/0011-plan-user-configured-task-channels.md);
- multiple servers/scheduling domains, resource capacity sharing, migration,
  richer networks, and richer GUI analysis need explicit designs;
- the current fixed GUI workbench has no docking, saved layout, native file
  dialog, unit-grouped/multiple signal axes, or background thread; timeline and
  signal viewport/filter preferences are not persisted; run-plan fields may be
  edited or loaded only while no run exists or the active run is
  Paused/Finished;
- architecture layout is deliberately simple and workspace pan/zoom is not
  persisted; graph task drops edit the pending plan and never migrate active
  jobs;
- display scale is captured at GUI startup; live per-monitor rescaling and
  persisted text preferences remain future presentation work;
- Bosch example execution currently uses one functional model and one of the
  validated single-vehicle `dedicated`/`shared_cloud` run plans; multi-vehicle,
  probabilistic timing, and three-core cloud experiments remain future work;
- MATLAB/Simulink remains a correctness oracle, not the simulator architecture.

End every handoff with exact commands/results, changed documents, limitations,
and the next permitted task. Update this page whenever the implemented project
boundary changes.
