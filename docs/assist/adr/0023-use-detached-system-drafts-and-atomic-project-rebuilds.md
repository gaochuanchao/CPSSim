# ADR-0023: Use Detached System Drafts and Atomic Project Rebuilds

- Status: Accepted
- Date: 2026-07-20
- Extended: 2026-07-20, Explorer-driven structural lifecycle
- Owners: CPSSim maintainers
- Related goal: Goal 2 System Builder

## Context

The GUI must edit immutable experiment configuration without exposing mutable
core containers or allowing a partial edit to alter a running simulation. A
system change can also invalidate the accepted run plan or fail while creating
a functional model, so replacing only the configuration would break the Goal 1
project/session ownership guarantee.

## Decision

`EditableSystemDraft` is a graphics-independent value owned by the GUI
application. It contains editable general timing, resource, task,
task-resource profile, and message-route rows. It has no pointer or reference
to `ExperimentConfig`, `ProjectContext`, or runtime state.

Dirty state is derived by comparing typed values with the draft's construction
baseline. New IDs use the smallest unused positive value. Renaming or changing
timing never changes an ID. Explorer deletion first computes the affected
profiles, incoming/outgoing routes, and pending assignments, then requires
explicit confirmation. Confirmation cascades only through the detached system
and run-plan drafts; Cancel changes nothing. Duplicating a task or resource
copies only that entity's values and never copies dependent profiles or routes.

Validation returns entity- and field-addressed diagnostics. A diagnostic-free
draft is converted through the existing `ResourceSpec`, `TaskSpec`, and
`ExperimentConfig` constructors, which remain the canonical semantic boundary.
The experiment JSON schema remains version 4.

System application uses this sequence:

1. build the canonical `ExperimentConfig`;
2. validate explicit builder-owned default assignments, the accepted horizon,
   and policy through `build_run_plan`;
3. construct a complete replacement `ProjectContext`, functional model, paused
   `GuiSimulationSession`, and controller; and
4. replace `GuiApplicationState` only after every earlier step succeeds.

The builder starts its default assignments from the accepted plan. Newly added
tasks remain explicitly unassigned until the user selects an accessible
profile. This preserves the run-plan ownership decision in ADR-0019.

`Save Project` serializes only the applied session configuration and accepted
plan. Unapplied edits are presentation/application state. Project replacement
with pending edits requires Apply and save, Discard, or Cancel. A valid draft
may be rendered as a clearly labelled, read-only architecture preview; the
architecture canvas does not edit system structure.

Bosch project systems remain adapter-owned and read-only. Goal 2 adds no Bosch
model-editing semantics.

Structural and runtime selection are separate values. Explorer selection uses
stable IDs or profile/route composite keys and controls only System Builder.
Timeline/event/resource selection controls only Runtime Inspector. Shared
highlighting may observe both domains but cannot overwrite either owner.

## Consequences

- invalid drafts, incompatible plans, and failed functional construction leave
  the active project/session unchanged;
- saving and reopening uses the existing strict project and configuration
  formats without persisting runtime state;
- forms and tables may evolve without moving semantic validation into ImGui;
- task additions require an explicit default assignment before Apply; and
- confirmed task/resource deletion can deliberately make the draft invalid
  until remaining tasks receive accessible profiles/assignments; and
- project-wide transactional disk persistence remains governed by the Goal 1
  safe-write behavior rather than by the editor.

## Alternatives considered

Mutating `ExperimentConfig` or runtime tasks in place was rejected because it
would mix immutable input with mutable execution state. Keeping editable JSON
inside widgets was rejected because it would duplicate parsing and lose typed
field diagnostics. Automatically choosing the first profile for new tasks was
rejected because run allocation is explicit experiment input.
