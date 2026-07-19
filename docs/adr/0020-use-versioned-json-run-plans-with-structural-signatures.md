# ADR-0020: Use Versioned JSON Run Plans with Structural Signatures

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: G03e, F1

## Context

ADR-0019 defines the typed in-memory `RunPlan`, shared validation boundary,
draft/active distinction, and atomic controller replacement. It deliberately
defers persistence because a file format also needs schema evolution,
experiment association, deterministic output, and GUI/CLI ownership rules.

G03e should let a user preserve an explicit assignment and horizon without
putting per-run choices into experiment JSON. Loading a plan must diagnose use
with the wrong experiment before changing the draft, and saving must never
serialize an incomplete or invalid draft. Persistence must remain usable by the
future F1 CLI without introducing Dear ImGui or filesystem behavior into the
simulation engine.

## Proposed decision

### Separate strict JSON document

Run plans use a separate strict JSON document with schema version 1. The first
schema is:

```json
{
  "schema_version": 1,
  "experiment_signature": {
    "tick_period_ns": 100000,
    "preemption": "preemptive",
    "resources": [
      {"id": 1, "name": "local"}
    ],
    "tasks": [
      {
        "id": 1,
        "name": "controller",
        "period_ticks": 10,
        "deadline_ticks": 10,
        "offset_ticks": 0,
        "priority": 1
      }
    ],
    "task_resource_profiles": [
      {"task_id": 1, "resource_id": 1, "execution_time_ticks": 2}
    ],
    "message_routes": []
  },
  "stop_tick": 300,
  "scheduling_policy": {"kind": "fixed_priority"},
  "task_assignments": [
    {"task_id": 1, "resource_id": 1}
  ]
}
```

Unknown fields, missing fields, wrong JSON types, unsupported schema versions,
negative stop ticks, unsupported policies, duplicate assignments, and invalid
strong-ID values are rejected. The shared `build_run_plan` performs assignment
completeness, resource existence, and accessible-profile validation after
syntax translation.

Every parse, schema, validation, and experiment-mismatch error identifies the
offending JSON location using a root-based path such as
`$.task_assignments[2].resource_id`. Collection-wide errors identify their
nearest owning path and the relevant strong ID.

Experiment JSON remains schema version 4. Run-plan schemas evolve separately
and do not add assignments, stop tick, or policy kind to `ExperimentConfig`.

### Structural experiment signature

The document records a canonical structural signature rather than an absolute
experiment path or an opaque implementation-dependent hash. The signature
contains every current simulation-relevant `ExperimentConfig` field:

- tick period and preemption mode;
- resource IDs and names;
- task IDs, names, timing, and priority;
- task-resource pairs and execution demand; and
- message-route endpoints and timing.

Collections are sorted by their stable strong identifiers before serialization
and comparison. Equivalent experiments with different declaration order have
the same signature. Any semantic field change produces an experiment-mismatch
diagnostic before the draft is replaced.

The explicit signature is intentionally verbose but transparent and
diagnosable. It avoids introducing a cryptographic dependency, relying on an
unstable `std::hash`, or treating a machine-specific source path as experiment
identity. A future manifest may additionally record source paths and
cryptographic checksums for provenance without changing plan compatibility.

### Shared persistence boundary

A non-GUI configuration translation unit exposes standard-library-only public
functions to:

- serialize an accepted `RunPlan` with its `ExperimentConfig` signature;
- parse text into a validated `RunPlan` for an expected experiment;
- load a plan file; and
- save a plan file.

The JSON library remains private to the implementation. GUI and future CLI
callers use the same parser, signature comparison, and `build_run_plan`
boundary. Allocators and `SimulationEngine` never open plan files.

Serialization is deterministic for identical typed inputs: object keys use the
library's stable ordering, collections use canonical strong-ID order, and one
trailing newline terminates the pretty-printed document.

### GUI lifecycle

The GUI saves the currently validated draft, not mutable runtime state. An
invalid draft fails before a file is opened for writing.

Load performs file access, strict parsing, signature comparison, and run-plan
validation before changing application state. On success it replaces the draft
fields only. The loaded plan remains pending until explicit Apply. On any
failure, the previous draft, active plan, controller, trace, and run state are
unchanged.

Loading is disabled while an active controller is Running, matching the G03
semantic-edit gate. Saving does not apply or reset a run. The initial GUI uses
an ImGui path-entry modal and adds no native file-dialog dependency. Workspace
preferences, recent-file lists, and autosave remain separate concerns.

## Consequences

Positive:

- plan files are portable, inspectable, strict, and independently versioned;
- exact experiment mismatch is detected without machine-specific paths;
- declaration order does not create a false mismatch;
- GUI and future CLI share one persistence and validation implementation;
- a failed load cannot partly change the draft or active run; and
- experiment configuration ownership remains unchanged.

Limiting:

- the structural signature repeats experiment metadata in every plan file;
- renaming a task or resource intentionally invalidates prior plan files;
- the initial GUI requires typed path entry rather than a native file picker;
- save-file replacement guarantees remain those of the standard file stream;
  and
- run provenance and output manifests remain future F1/F2 work.

## Alternatives considered

### Store only task and resource IDs

Rejected because a plan could silently apply after timing, execution demand,
scheduling, route, or tick-duration changes and appear to reproduce a different
experiment.

### Store the experiment source path

Rejected because paths are machine-specific, files can change in place, and
programmatically constructed experiments have no source path.

### Store a non-cryptographic hash

Rejected because common library hashes are not stable persistence contracts and
an opaque mismatch is less actionable than an inspectable signature.

### Add SHA-256 now

Deferred because it adds implementation/dependency surface while still
requiring canonical experiment serialization. A future provenance manifest can
hash the canonical configuration and source files when reproducibility records
are implemented.

### Embed the full experiment as the runnable input

Rejected because it would combine experiment capabilities and per-run choices,
duplicating the ownership that ADR-0002, ADR-0006, and ADR-0019 keep separate.

### Add a native file-dialog dependency

Deferred because path-entry controls complete G03e without expanding graphics
dependencies or cross-platform packaging decisions.

## Approval gate

No G03e persistence implementation begins while this ADR remains Proposed.
After the schema, structural signature, shared API, and GUI load/save lifecycle
are approved, change the status to Accepted and implement G03e only.
