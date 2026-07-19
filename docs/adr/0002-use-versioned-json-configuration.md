# ADR-0002: Use a Strict Versioned JSON Configuration Schema

- Status: Accepted
- Date: 2026-07-17
- Amended: 2026-07-18 for schema versions 2–4 and legacy compatibility

## Context

CPSSim needs a portable experiment format that can be inspected, versioned,
tested, and loaded without MATLAB or Bosch-specific types. Configuration errors
must fail before mutable simulator state exists. The schema must preserve the
integer-time decision in ADR-0001 and leave room for later execution-time
models without designing them prematurely.

## Decision

The experiment format is strict, explicitly versioned JSON. Version 1 remains
accepted for compatibility with configurations created during the initial T4
implementation.

Version 1 contains:

- a positive integer `tick_period_ns`;
- a nonempty resource array with unique unsigned IDs and unique nonempty names;
- a nonempty task array with unique unsigned IDs and unique nonempty names;
- integer period, deadline, offset, and priority fields;
- a resource reference for each task; and
- an execution-time object whose only supported kind is `deterministic` with a
  positive integer tick count.

Unknown fields, missing fields, wrong JSON types, and unsupported schema
versions are rejected. Execution demand must not exceed relative deadline.
Deadlines are not required to be no greater than periods, and equal priorities
are allowed.

The JSON library is isolated in `src/cpssim/config/json_config.cpp`. Public
model and parser headers use CPSSim and standard-library types only.
nlohmann/json 3.11 is located through CMake, with a SHA-256-pinned v3.11.3
FetchContent fallback when a system package is unavailable.

Version 2 separates resource-independent task timing from possible runtime
resource assignments:

- task entries contain identity, period, deadline, offset, and priority;
- a top-level `task_resource_profiles` array contains unique task-resource
  pairs and deterministic execution time for each pair; and
- every task must have at least one valid profile.

The version-1 loader translates each task's former fixed `resource_id` and
`execution_time` into one separated model profile. This amendment implements
the ownership decision in ADR-0006.

Version 3 adds a required top-level `scheduling` object containing
`preemption`, whose accepted values are `preemptive` and `non_preemptive`.
Versions 1 and 2 are translated to preemptive mode, preserving the only
runtime behavior available when those schemas were current. Version 3 was the
current output format for T10. This amendment implements ADR-0009.

Version 4 adds a required top-level `message_routes` array. Each entry has a
source task ID, destination task ID, positive integer send offset, and positive
integer fixed delay. Endpoint pairs are unique and must refer to configured
tasks. Versions 1–3 translate to an empty route array, preserving the absence
of network behavior in files created before T11. New configurations use
version 4. Message lifecycle and causal event behavior are fixed separately by
[ADR-0010](0010-generate-task-routed-messages-with-fixed-delay.md).

## Consequences

- Configurations use deterministic integer time and are portable across the
  current supported build environments.
- Typos cannot silently become ignored configuration fields.
- Incompatible schema evolution requires a new version and explicit parser
  support.
- Old version-1 files keep their single fixed mapping; the loader does not
  invent extra resource choices.
- Old version-1 and version-2 files keep their historical preemptive behavior.
- Old version-1 through version-3 files do not acquire message routes.
- Future stochastic or trace-driven execution models can use a different
  `execution_time.kind`, but T4 rejects them until their semantics and seed
  rules exist.
- Consumers can construct model specifications without depending on JSON.
- A first build may require network access when nlohmann/json is not installed
  and no verified dependency cache exists.
- Experiment horizon, selected runtime assignment, concrete policy type, and
  Bosch trigger mapping remain outside configuration. The global preemption
  assumption and generic fixed routes are now explicit configuration.

## Alternatives considered

### Floating-point seconds in JSON

Rejected because canonical task timing must use exact integer ticks. Physical
time is represented only by the integer nanoseconds-per-tick boundary fixed by
ADR-0001.

### Ignore unknown fields

Rejected because a misspelled timing or mapping field could otherwise appear
to load successfully while changing experiment meaning.

### Put JSON objects in model interfaces

Rejected because it would couple portable model records and future runtime code
to one serialization library.

### Write a project-specific JSON parser

Rejected because standards-compliant parsing is not part of CPSSim's research
goal and would add unnecessary correctness and maintenance risk.
