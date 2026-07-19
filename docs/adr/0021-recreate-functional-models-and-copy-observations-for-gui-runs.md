# ADR-0021: Recreate Functional Models and Copy Observations for GUI Runs

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related task: G06

## Context

G06 plots the append-only typed observations already owned by
`FunctionalRuntime`. The GUI must receive those values without reading live
runtime containers, and Reset must clear both series and external-model state.
A `FunctionalModel` object cannot generally be initialized twice: the mock and
Bosch FMI implementations both own one-run lifecycle state.

Signal display metadata is also adapter knowledge. The generic observation
contract provides typed stable names and values but does not provide localized
labels, hierarchy, units, or an adapter identity. Adding Bosch names or FMI
value references to generic GUI support would reverse the intended dependency.

## Decision

`GuiSimulationSession` optionally owns a copyable functional-model factory and
a detached signal-presentation registry. Each accepted Apply or Reset asks the
factory for one fresh `FunctionalModel`. `SimulationController` owns that model
for the run; `SimulationEngine` and its `FunctionalRuntime` retain their
existing non-owning reference and append-only trace ownership.

`SimulationSnapshot` copies:

- whether a functional model is attached;
- optional GUI signal descriptors; and
- the current validated `FunctionalObservation` trace.

The copy is detached. Rendering cannot mutate the engine, functional runtime,
external model, or future snapshots.

A signal's generic stable identity is its scalar type plus its adapter-defined
source name. Optional descriptors map that identity to a hierarchical path,
display name, unit, and source identity. When no descriptor is supplied, GUI
support uses the source name and type with an empty unit. Generic GUI support
contains no Bosch or FMI mapping.

`GuiSignalCache` validates one schema and appends only new observation rows.
Reset, trace shrink, registry change, or a changed trace boundary rebuilds it.
Rendering may convert typed values and integer ticks to floating-point screen
coordinates, but stored series retain their typed values and canonical integer
ticks.

The initial plot uses Dear ImGui draw lists. No new plotting dependency is
added. Deterministic visual downsampling preserves visible endpoints plus
per-bucket minima and maxima and never modifies full-resolution series.

## Consequences

Positive:

- Reset cannot reuse stale external-model lifecycle state;
- snapshots remain safe detached values;
- signal extraction and downsampling remain graphics-independent and tested;
- adapters may provide labels and units without entering generic GUI support;
- GUI refresh timing cannot affect functional progression; and
- headless builds gain no graphics dependency.

Limiting:

- a factory must be able to recreate all immutable adapter input for each run;
- snapshots still copy the complete functional trace, matching the existing
  clarity-first canonical-event snapshot policy;
- integer values outside exact `double` range may be approximated only in plot
  coordinates, while the stored sample and cursor text remain integral; and
- the custom view has one shared value axis rather than unit-grouped axes.

## Alternatives considered

### Reuse one functional model after Reset

Rejected because functional models have one-run initialization/finalization
state and FMI instances require clean recreation.

### Expose `FunctionalRuntime::trace()` directly to the renderer

Rejected because a live reference could be invalidated during progress and
would bypass the snapshot boundary accepted by ADR-0018.

### Add display metadata to `FunctionalObservation`

Rejected because observation traces are semantic runtime data, while labels,
units, and hierarchy are replaceable presentation metadata.

### Add ImPlot immediately

Rejected because G06a-G06c requirements are satisfied by a small custom view;
there is not yet evidence that another pinned dependency is necessary.

## Validation

- no-model snapshots are explicit and empty;
- observation and registry copies cannot mutate controller state;
- Reset creates a fresh model and clears observations;
- GUI and headless canonical traces remain identical;
- full and incremental signal builds compare exactly;
- registry and schema failures identify the observation, tick, and signal;
- deterministic downsampling preserves endpoints and extrema; and
- Debug, Release, sanitizer, GUI build, and runtime smoke checks pass.
