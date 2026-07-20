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
workbench with automatic current-monitor DPI scaling, adjustable text, queued
controls, an Experiment Explorer, shared strong-ID selection, read-only inspection,
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
The GUI now starts on a session-free Home screen and owns an optional project
context. Native/platform-neutral dialogs drive generic project creation,
atomic open/save-as, run-plan load/save, and custom Bosch trajectory selection.
The Bosch GUI factory builds a validated paused project without executing the
simulation; recent projects live in a separate bounded user-preference file,
and invalid minimal workspace presentation state falls back safely.
Generic projects now provide a complete forms-and-tables System Builder over a
detached typed draft. Resources, tasks, execution profiles, routes, and
explicit default assignments validate before one atomic paused project/session
replacement; valid unapplied drafts can drive a read-only architecture preview.
Explorer now owns structural create/duplicate/confirmed-cascade actions and
drives the selected property editor below it. Structural selection is separate
from runtime selection; Run Configuration and Runtime Inspector occupy the
split right sidebar without duplicating structural properties.
The repository command surface is limited to `make`, `make run-cli`,
`make run-gui`, `make test`, `make clean`, and `make help`. The CLI provides a
registered persistent shell and direct commands; its Bosch wizard/direct path
shares `DefaultBoschRunService` with the internal compatibility executable.
`scripts/verify.sh` owns interactive/direct verification selection, and CTest
primary module labels replace specialized public Make targets.

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
| `EditableSystemDraft` | detached system fields, stable ID allocation, mutation policy, and structured diagnostics |
| `StructuralSelection` | Explorer section/entity/profile/route identity controlling System Builder |
| `SystemExplorerInteraction` | headless create/duplicate/cascade-confirm lifecycle and focus/scroll intent |
| runtime `Task` | applied assignment and next release |
| `EventQueue` | pending candidates and sequence allocation |
| `SchedulingPolicy` | read-only ranking/preemption recommendation |
| `Scheduler` | jobs, Ready queues, transitions |
| `Resource` | active interval and busy-time accounting |
| `FixedDelayNetwork` | messages and lifecycle |
| `SimulationEngine` | current event tick, routing, accepted trace |
| `FunctionalRuntime` | functional lifecycle and observation trace |
| `BoschFmi2FunctionalModel` | immutable Bosch trajectory copy and FMI component lifecycle |
| `DefaultBoschRunService` | parser-independent supplied-trajectory run orchestration |
| `CommandRegistry` / `CliApplication` | CLI command collection, dispatch, and shell lifecycle |
| `SimulationController` | GUI commands, current GUI-owned functional model, and detached snapshots |
| `GuiSimulationSession` | loaded experiment, functional-model factory/registry, draft validation, and atomic active-controller replacement |
| `ExperimentPresentationSnapshot` | sorted detached configuration and applied assignment copy |
| `GuiSelection` | runtime highlight/inspection identity and optional tick range |
| `GuiTimelineCache` | disposable validated timeline prefix and derived lifecycle presentation |
| `GuiSignalCache` | disposable validated functional schema and full-resolution derived scalar series |
| `GuiApplication` | panel visibility, view state, text scale, fixed workbench layout, and About-dialog state |
| `GuiApplicationState` | Home or one owned standalone/project Workbench context |
| `ProjectContext` | project root/metadata, loaded specifications/workspace, runtime factory inputs, and sole GUI session |
| `RecentProjects` | normalized bounded GUI user-preference history |
| `system_builder_workflow` | canonical config/plan validation and atomic project/session reconstruction |

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
./scripts/verify.sh quick
./scripts/verify.sh full
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
- F1 still requires general JSON experiment/allocation execution and canonical
  trace/manifest output; the registered CLI and supplied Bosch run path are
  implemented foundations, not completion of that broader proposal;
- one directed channel and task port per configured task connection is planned
  in [ADR-0011](../adr/0011-plan-user-configured-task-channels.md);
- multiple servers/scheduling domains, resource capacity sharing, migration,
  richer networks, and richer GUI analysis need explicit designs;
- the current fixed GUI workbench has no docking,
  unit-grouped/multiple signal axes, or background thread; timeline and
  signal viewports and text scale are not persisted; run-plan fields may be
  edited or loaded only while no run exists or the active run is
  Paused/Finished;
- architecture layout is deliberately simple and workspace pan/zoom is not
  persisted; graph task drops edit the pending plan and never migrate active
  jobs;
- current-monitor display scale updates automatically; theme changes rebuild
  an unscaled base style before that scale is applied once; workspace schema 2
  persists panel/splitter/tab/event-filter/selected-signal presentation state;
- Bosch example execution currently uses one functional model and one of the
  validated single-vehicle `dedicated`/`shared_cloud` run plans; multi-vehicle,
  probabilistic timing, and three-core cloud experiments remain future work;
- MATLAB/Simulink remains a correctness oracle, not the simulator architecture.

End every handoff with exact commands/results, changed documents, limitations,
and the next permitted task. Update this page whenever the implemented project
boundary changes.

## Latest Goal 3 workbench validation

The completed Goal 3 refinement was validated on 2026-07-20 with:

- `cmake --build build/make-dev --target cpssim_gui_tests cpssim_project_tests cpssim_gui -j2`:
  passed;
- focused GUI/project CTest selection: 100/100 passed;
- `./scripts/verify.sh full`: formatting plus Debug, ASan/UBSan, Release, and
  Clang profiles passed all 237 tests; the initial clang-tidy pass identified
  three new API/copy warnings, which were corrected;
- the corrected clang-tidy build and its 237/237 tests passed; and
- `cpssim_gui --help`, final formatting check, and `git diff --check` passed.

The automated environment has no usable X11 display, so narrow/wide window,
splitter reachability, theme switching, mixed-DPI monitor movement, native
dialog appearance, and large Bosch trace scrolling still require a manual
desktop smoke test. No additional package is required for those checks.

## Latest Extended Goal 2 System Builder validation

Extended Goal 2 was implemented on 2026-07-20. Focused validation covered
canonical round trips, minimal drafts, dirty state, deterministic IDs,
Explorer creation/duplication/focus intent, disabled additions, confirmed
cascade impact/execution/cancel, independent selection domains, profiles/routes,
structured diagnostics, atomic rebuild failures, pending run settings,
transition decisions, and save/reopen behavior:

- `cmake --build --preset gui --target cpssim_gui -j2`: passed;
- `ctest --test-dir build/dev --output-on-failure -L "^(gui|project)$"`:
  92/92 focused tests passed, including current DPI tests;
- `cmake --build --preset tidy --target cpssim_gui_support
  cpssim_project_tests -j2`: passed; and
- `./scripts/verify.sh full`: formatting plus Debug, ASan/UBSan, Release,
  Clang, and clang-tidy profiles passed all 226 tests, including CLI, DPI,
  canonical ordering, Bosch functional output, and conformance regressions;
- `make`: built the documented CLI, GUI, Bosch adapters, and FMU targets; and
- `./build/make-dev/cpssim_cli help`: passed and listed the registered command
  surface.

The automated environment still has no usable X11 display, so context menus,
focus/scroll behavior, splitters, native prompts, keyboard traversal, and
monitor switching require a manual desktop smoke test. Bosch systems remain
intentionally read-only.

## Latest Goal 1 project-workflow validation

The completed Goal 1 workflow was validated on 2026-07-20 with:

- `cmake --build --preset gui --target cpssim_gui -j2`: passed;
- `ctest --test-dir build/dev --output-on-failure -L "^(gui|project)$"`:
  68/68 focused GUI and project tests passed, including DPI/monitor behavior;
- `./scripts/verify.sh full`: formatting and all 202 tests passed under Debug,
  ASan/UBSan, Release, Clang, and clang-tidy; and
- `git diff --check`: passed.

The automated environment has no usable X11 display, so the native dialogs and
visual monitor switching require a manual desktop smoke test. No additional
development package is required: portable-file-dialogs is pinned and fetched
only for the GUI build.

## Latest command-interface validation

The Make/CLI refinement was validated on 2026-07-20 with:

- `make`: built `cpssim_cli`, `cpssim_gui`, their core/adapter dependencies,
  and `cpssim_bosch_fmu_linux` without building test executables;
- `printf 'exit\n' | make run-cli`: built required targets and opened/closed
  the persistent `CPSSim 0.1.0` shell cleanly;
- `./build/make-dev/cpssim_cli run bosch --example
  examples/example_v_10 --scenario shared_cloud --stop-tick 2`: completed via
  the real FMU with 9 canonical events and 3 functional observations;
- interactive `make test`: displayed the seven-choice menu, listed all 11
  maintained module labels, and exited cleanly;
- `./scripts/verify.sh all`: 178/178 Debug tests passed before the tidy cleanup;
- `./scripts/verify.sh module fmi`: 7/7 passed; `module bosch`: 4/4 passed;
- follow-up `./scripts/verify.sh full`: formatting and all 179 tests passed
  under Debug, ASan/UBSan, Release, Clang, and clang-tidy after guarded
  optional access was added across run-plan persistence and GUI caches;
- `git diff --check` and `./scripts/verify.sh format-check`: passed; and
- `make clean`: removed only the documented generated directories; sources,
  docs, configurations, examples, and Bosch references remained present.

Changed current documentation: root `README.md`, `docs/COMMANDS.md`,
`docs/README.md`, `docs/MODULE-INTERACTIONS.md`, the developer/project/handoff
guides, GUI tutorial, F1 future-work boundary, architecture/roadmap
instructions, FMI/functional/application module notes, Bosch example usage,
and ADR-0022. The tidy follow-up also updated the experiment-configuration and
GUI architecture pages. The dated Bosch devlog only gained a notice that its
former command names are historical.

Limitations: `make run-gui` built `cpssim_gui` successfully but GLFW could not
open the environment's `:0` X11 display, so no visual smoke was possible. The
clang-tidy limitation is resolved: production targets are analyzed cleanly;
Catch2 test targets remain warning-clean and execute in the tidy preset but are
not analyzed because Catch2's assertion decomposition produces false chained-
comparison diagnostics.

No next task is selected. The next permitted F1 step, only when explicitly
requested, is general JSON experiment/allocation execution with canonical
trace and run-manifest output; do not infer it from this interface refactor.
