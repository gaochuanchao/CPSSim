# ADR-0006: Assign Resources to Runtime Tasks

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T4, T5, T8, T9, T10

## Context

The initial `TaskSpec` contained one `resource_id` and one execution time. That
made resource placement immutable configuration even though placement is a
separate allocation decision. It also could not represent a task that is executable on
multiple resources with different execution demands.

CPSSim needs a simple boundary that supports the one-resource baseline without
preventing later local/cloud assignment. T8 introduced assignment mechanism
without a selection policy. T9 now needs a common decision boundary for
single-resource, file-configured, and algorithmic placement.

## Decision

`TaskSpec` contains task identity, periodic timing, and priority only.
`ExperimentConfig` separately owns `TaskResourceProfile` values. Each profile
identifies an accessible `(TaskId, ResourceId)` pair and the deterministic
execution time for that pair. Every task has at least one profile.

Runtime `Task` owns an optional current resource assignment. The simulation
engine applies a resource allocator's choice through `assign_resource`; the
`Task` validates that the resource is accessible. Runtime job schedulers do not
choose this task placement.

### Allocation interface

`ResourceAllocator` is a read-only base interface. Given an immutable
`ExperimentConfig`, it returns a complete vector of `TaskAssignment` records.
Each record contains one `TaskId` and selected `ResourceId`. The engine checks
that every configured task appears exactly once and applies the plan before
initial releases.

`SingleResourceAllocator` is the T9 implementation. It requires exactly one
configured resource, verifies that every task can execute there, and assigns
all tasks to it.

`ConfiguredResourceAllocator` is the T10 implementation for explicit mapping.
It copies caller-provided `TaskAssignment` records; the engine validates the
complete plan before applying it.

Future implementations may calculate a global placement algorithmically or
use assignments supplied by a user. File parsing remains in the configuration
layer: a configured allocator consumes validated records rather than opening a
JSON or other user file itself.

When a task schedules a release, that pending job captures the current
assignment. When the release is processed, the returned `JobState` copies the
captured resource and that profile's execution time. Reassignment affects the
next release scheduled afterward; it does not change a pending or released job.
Job migration is therefore not supported by this baseline.

Runtime `Resource` owns a copied `ResourceSpec` and active Running execution
state. Runtime `Scheduler` owns Ready membership and coordinates transitions.
The initial resource model remains exclusive, with at most one Running job.
Fractional/spatial capacity is deferred until allocation and progress
semantics are defined. This ownership refinement is recorded in ADR-0009.

JSON schema version 2 expresses tasks and profiles separately. Version 1 is
translated into one profile per legacy fixed task mapping. Schema version 3
retains this separation and adds experiment-wide scheduling configuration.

## Consequences

Positive:

- allocation policy can choose among declared accessible resources;
- task placement and runtime job scheduling can evolve independently;
- a complete plan supports algorithms that consider all tasks together;
- execution demand can differ by task-resource pair;
- immutable specifications remain separate from mutable placement;
- each job has a stable resource and execution demand; and
- future multi-resource policies have a direct configuration input.

Limiting:

- every in-horizon task must be assigned before its first release;
- configured assignments currently require caller-constructed records;
- reassignment is not migration;
- execution demand is currently deterministic; and
- capacity sharing and simultaneous jobs on one resource require a later
  design.

## Alternatives considered

### Keep the selected resource in `TaskSpec`

Rejected because it makes an allocation decision immutable and prevents an
allocator from choosing among resources.

### Assign every job independently after release

Rejected for the current baseline because the allocator assigns a runtime task
and its newly generated jobs inherit that task-level decision.

### Let each allocator parse its own user file

Rejected because allocation algorithms should not depend on JSON, filesystem
access, or a specific configuration representation. Parsing and validation
belong to the configuration layer.

### Add fractional resource capacity now

Deferred because merely adding a capacity field does not define allocation,
execution progress, preemption, or completion behavior.

## Validation

Tests verify inaccessible/missing assignments, per-resource execution lookup,
assignment capture, reassignment effects on successors, profile references and
uniqueness, version-1 translation, version-2 example loading, the
single-resource plan, and engine rejection of incomplete plans.

T10 tests additionally verify configured-plan retention, unknown and
inaccessible mapping rejection, allocator visibility of the shared scheduling
assumption, and independent execution on the chosen resources.
