# ADR-0003: Use Versioned JSON Lines for Canonical Event Records

- Status: Accepted
- Date: 2026-07-18
- Owners: Chuanchao Gao
- Related tasks: T6, T7, T8, T9, T11, T12
- Amended: 2026-07-18 for T11 message-event vocabulary

## Context

CPSSim needs a portable canonical event record that can be compared across
runs, replayed by later tools, and translated into CSV or adapter-specific
formats. It must preserve exact integer time, append identity, causal links,
and stable entity references without depending on MATLAB tables, pointer
addresses, Bosch trigger columns, or one scheduling policy.

T6 defines a record and its serialization. T7 will define queue ordering and
sequence assignment, so T6 must not silently turn enum declaration order into
the scheduling order.

## Decision drivers

- Byte-stable output is required for deterministic trace comparison.
- Records must remain readable during scientific debugging and review.
- Time, sequences, and IDs must retain exact integer values.
- Optional entity domains and causal links need an unambiguous representation.
- Schema evolution must be detectable rather than inferred.
- The portable model API must not expose JSON-library types.

## Considered options

### Fixed versioned JSON object per line

Each event is one compact JSON object followed by one line-feed character. The
object uses fixed keys, field order, enum spellings, and explicit null values.

Advantages:

- readable and streamable;
- deterministic byte representation can be tested directly;
- one damaged line does not obscure record boundaries;
- optional fields and later schema versions are explicit; and
- existing nlohmann/json dependency can remain implementation-private.

Disadvantages:

- more verbose than a binary format;
- exact field order becomes part of the canonical writer contract; and
- schema changes require deliberate versioning.

### CSV as the canonical format

Advantages:

- familiar to MATLAB and spreadsheet tools;
- compact for a fixed flat schema.

Disadvantages:

- awkward for typed optional references and later event payloads;
- escaping and missing-value conventions are easy to vary; and
- schema evolution is less explicit.

CSV remains appropriate as a derived analysis projection.

### Event-specific variant payloads

Advantages:

- each event type could require exactly its relevant fields.

Disadvantages:

- T6 would have to decide payload semantics owned by later release,
  scheduling, network, and adapter tasks; and
- the initial implementation and schema would be substantially more complex.

A later schema version may add event-specific payloads when concrete behavior
requires them.

## Decision

The canonical T6 record is `Event` with:

- nonnegative signed integer `Tick`;
- `EventPhase`;
- unsigned `EventSequence`;
- `EventType`;
- `EventEntityRefs`, containing optional typed task, job, resource, message,
  and vehicle IDs; and
- optional causal `EventSequence`.

A present causal sequence must be strictly less than the event's own sequence.
T6 accepts the full unsigned sequence domain and does not choose whether T7
starts allocation at zero or one.

`EventPhase` provides these semantic names:

- `ExecutionCompletion`;
- `MessageDelivery`;
- `DeadlineCheck`;
- `JobRelease`;
- `PolicyUpdate`;
- `Scheduling`; and
- `CausedAction`.

Their enum declaration order is not canonical queue precedence. T7 must define
and test phase ordering through its own ADR and comparator.

`serialize_event_json_line` emits schema version 1 with this exact top-level
field order:

1. `schema_version`;
2. `tick`;
3. `phase`;
4. `sequence`;
5. `type`;
6. `entities`; and
7. `cause_sequence`.

The nested `entities` order is `task_id`, `job_id`, `resource_id`,
`message_id`, then `vehicle_id`. Absent references and causes are explicit JSON
`null`. Enum values use fixed lowercase snake-case strings. Numeric time,
sequence, and identifiers remain JSON integers. Output is compact and ends
with exactly one `\n` byte.

The public headers expose CPSSim and standard-library types only.
nlohmann/json's `ordered_json` is confined to the serializer implementation.

T11 adds `message_send` and `message_delivery` as event-type spellings within
the same version-1 record shape. A send identifies its source task, source job,
and message. A delivery identifies its destination task and message. The send
cause is the accepted `JobFinish` sequence; the delivery cause is the
`MessageSend` sequence. No new serialized field or schema version is needed.

## Consequences

Positive:

- equivalent events produce byte-identical lines;
- causal and entity references are portable stable values;
- traces can be streamed, diffed, and processed line-by-line;
- schema incompatibility is visible; and
- model code remains independent of the JSON implementation.

Negative or limiting:

- all optional entity keys are written even when absent;
- T6 does not validate event-type-specific reference combinations;
- no deserializer or whole-trace writer exists yet; and
- changing key order, spelling, or null policy requires an intentional schema
  decision and compatibility work.

## Validation

- Construction tests cover retained fields, negative ticks, and invalid causal
  direction.
- Exact-string tests cover present references, absent references, enum names,
  key order, schema version, compact output, and the final newline.
- Debug, Release, sanitizer, Clang, and clang-tidy builds must pass.

## Follow-up

- T7 defines sequence assignment and stable queue ordering by tick, phase, and
  sequence through a separate ADR.
- T8/T9 define release and scheduling emission contracts. T11 defines the
  message send/delivery contracts above.
- A later trace component owns append-only storage and file I/O.
- Deserialization or schema evolution requires explicit tests and, if
  compatibility changes, a new ADR or superseding decision.
