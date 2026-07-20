# Module: Experiment Configuration

## Purpose

Translate versioned JSON into portable immutable CPSSim specifications and
validate separate typed choices for one simulation run.

## Public model

- `ResourceSpec`: stable resource ID and name.
- `PeriodicTimingSpec`: period, relative deadline, and first offset.
- `TaskSpec`: stable task identity, periodic timing, and priority; no selected
  resource.
- `TaskResourceProfile`: accessible `(TaskId, ResourceId)` pair and
  deterministic execution time.
- `SchedulingSpec`: experiment-wide preemptive/non-preemptive behavior.
- `MessageRouteSpec`: completion-triggered source/destination task pair with
  positive send offset and fixed delay.
- `ExperimentConfig`: validated owner of scheduling, resources, tasks,
  profiles, and routes.
- `RunPlanRequest`: possibly invalid assignments, inclusive stop tick, and
  scheduling-policy kind supplied by an application.
- `RunPlan`: immutable validated per-run assignments, stop tick, and policy
  kind, canonicalized into configured task order.

The selected resource is runtime `Task` state. See
[ADR-0006](../adr/0006-assign-resources-to-runtime-tasks.md).

## GUI presentation

`GuiSimulationSession` owns a `RunPlanDraft` separately from its optional active
`SimulationController`. A new draft has one unassigned field per task; the GUI
does not infer the first accessible profile. The shared non-GUI run-plan
builder rejects incomplete, duplicate, unknown, inaccessible, negative-horizon,
or unsupported-policy input before engine construction. Apply constructs a
complete replacement controller before swapping it into the session, so a
failed Apply preserves the previous run. Reset reads only the controller's
accepted immutable plan, never pending draft values.

`ExperimentPresentationSnapshot` can describe a loaded experiment before a run
exists and adds applied assignments after successful Apply. Resources, tasks,
profiles, routes, and active assignments are copied into strong-ID records and
sorted by documented identifier keys. The Explorer, run-plan editor, and
Inspector render those copies without reparsing JSON or receiving mutable
configuration/runtime access.

## Invariants

- Tick period is positive.
- Task/resource collections are nonempty with unique IDs and names.
- Every profile refers to defined records and every pair is unique.
- Profile execution is positive and no greater than the task deadline.
- Every task has at least one accessible resource.
- Every route refers to known tasks, uses positive timing, and has a unique
  endpoint pair.
- Located run-plan errors validate the identifier payload required by each
  diagnostic code before converting it to a JSON path; accepted build results
  likewise prove that a plan is present before returning it.
- Public collection views are read-only.

## JSON contract

Version 4 retains separated `tasks`, `task_resource_profiles`, and explicit
`scheduling.preemption`, then adds required `message_routes`. Versions 1–3
remain read-compatible and translate to no routes; versions 1 and 2 also keep
their historical preemptive behavior, and each version-1 fixed task mapping
becomes one profile. Unknown fields, wrong types, missing fields, and
unsupported versions are rejected.

JSON-library types do not appear in public model headers. The parser currently
supports deterministic execution only.

Run plans are deliberately separate from experiment JSON. Run-plan JSON schema
version 1 stores the stop tick, fixed-priority policy kind, strong-ID task
assignments, and a canonical structural experiment signature. The signature
contains every current simulation-relevant configuration field in stable-ID
order, so declaration reordering is compatible while semantic changes are
reported as experiment mismatches. Experiment schema version 4 is unchanged.

Run-plan parsing is strict and reports every malformed, schema, association, or
assignment error at a root-based JSON path such as
`$.task_assignments[1].resource_id`. GUI Load replaces only the pending draft;
Apply remains explicit. GUI Save validates the draft before opening the output
file. Both operations use the same non-GUI API intended for the future CLI.

## Main files

- [specifications.hpp](../../src/cpssim/model/specifications.hpp)
- [experiment_config.hpp](../../src/cpssim/model/experiment_config.hpp)
- [run_plan.hpp](../../src/cpssim/model/run_plan.hpp)
- [json_config.cpp](../../src/cpssim/config/json_config.cpp)
- [json_run_plan.hpp](../../src/cpssim/config/json_run_plan.hpp)
- [ADR-0002](../adr/0002-use-versioned-json-configuration.md)
- [ADR-0019](../adr/0019-use-typed-run-plans-and-atomic-gui-application.md)
- [ADR-0020](../adr/0020-use-versioned-json-run-plans-with-structural-signatures.md)

## Verification

[specifications_test.cpp](../../tests/model/specifications_test.cpp) covers
local/cross-record invariants. [json_config_test.cpp](../../tests/config/json_config_test.cpp)
exercises all four schema versions, strict rejection, and the tracked version-4
example. [presentation_model_test.cpp](../../tests/gui/presentation_model_test.cpp)
checks complete detached GUI records, applied-assignment uniqueness, and
declaration-order-independent presentation. [run_plan_test.cpp](../../tests/model/run_plan_test.cpp)
checks shared validation and canonicalization;
[draft_run_plan_test.cpp](../../tests/gui/draft_run_plan_test.cpp) checks
explicit drafts, atomic Apply, Reset ownership, dirty state, and the Running
edit gate. [json_run_plan_test.cpp](../../tests/config/json_run_plan_test.cpp)
checks byte-deterministic round trips, declaration-order independence, exact
error locations (including unknown-task and inaccessible-resource
diagnostics), structural association, and file I/O.
