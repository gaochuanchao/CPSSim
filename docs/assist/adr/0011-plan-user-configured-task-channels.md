# ADR-0011: Plan One User-Configured Channel per Directed Task Pair

- Status: Proposed
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: future channel/data-version task, T11, T12, FMI integration

## Context

T11 implements the fixed positive timing needed for Bosch conformance through
`MessageRouteSpec`, `Message`, and `FixedDelayNetwork`. A later general model
must also represent persistent logical connections between tasks, actual data
availability, and the time at which a job reads and writes data.

A route or channel and a message are not the same concept. A channel exists
for the experiment and defines connectivity and timing. A message or data
version is one runtime transfer produced by a particular job. Tasks also need
explicit input and output ports so writes, deliveries, and reads have stable
endpoints rather than referring only to an entire task.

The future model must remain deterministic without inferring connectivity from
resource placement, task names, Bosch triggers, or FMI variables. It must also
make zero-delay read/write ordering explicit rather than relying on container
insertion order.

## Proposed decision

### User-configured directed connectivity

Experiment input defines the connected task pairs. CPSSim does not create a
channel merely because two tasks use the same resource, server, or scheduler.

Channels are strictly single-directional. There is at most one channel for
each directed pair `(source TaskId, destination TaskId)`. Therefore:

- `A -> B` and `B -> A` are different directed pairs and may both exist;
- parallel channels from `A -> B` are not allowed; and
- if several data items later travel from `A -> B`, their data/payload model
  must share that one connection rather than create duplicate channels.

The configuration supplies at least the source task, destination task, and
channel timing. A stable `ChannelId` may be added for trace and runtime
identity, but it does not permit duplicate endpoint pairs.

### One port at each end of every channel

Each directed channel connects exactly one source-task output port to exactly
one destination-task input port:

```text
Task A / OutputPort -> directed Channel -> InputPort / Task B
```

The port cardinality follows the configured graph:

- every outgoing channel of a task has its own output port;
- every incoming channel of a task has its own input port;
- one output port connects to exactly one channel;
- one input port connects to exactly one channel; and
- a task with no outgoing or incoming channels has no corresponding ports.

Thus a task with three outgoing and two incoming channels has three output
ports and two input ports. Fan-out uses several output-port/channel pairs, and
fan-in uses several channel/input-port pairs. It is never hidden inside one
port.

Ports belong to their tasks. The likely complete identities are
`(TaskId, OutputPortId)` and `(TaskId, InputPortId)`, using distinct strong ID
types so an input endpoint cannot accidentally be supplied where an output is
required. Whether the JSON schema lists ports inside tasks or derives them
from channel declarations remains an implementation-format decision; either
representation must enforce the same one-to-one port/channel relationship.

### Persistent channel and runtime transfers

The future channel is the persistent owner of connection and in-transit state.
A `Message` or `DataVersion` represents one transfer through it and refers
back to that channel. Ports give committed and delivered data a clear owner.

The intended ownership is:

| State | Owner |
|---|---|
| Immutable task ports, connectivity, and delay | `ExperimentConfig` task/channel specifications |
| Latest successfully committed output version | Runtime output port |
| In-transit message/data version | Runtime channel component |
| Latest delivered and readable version | Runtime input port |
| Pending availability events | Global `EventQueue` |
| Job input snapshot | The consuming job or functional-execution record |
| Global time and event routing | `SimulationEngine` |

`Scheduler`, `SchedulingPolicy`, and `Resource` do not own channel data.

### Channel timing

A logical connection may have zero delay. A network connection may have a
positive delay and may later gain a richer transport model. All canonical
times remain integer ticks.

The planned task data contract is:

1. a producer writes/commits each relevant output port only after successful
   job completion;
2. channel delivery updates its connected destination input port;
3. a consumer reads or snapshots its input ports when its job starts;
4. the snapshot does not change while that job executes; and
5. at the same tick on a zero-delay channel, the successful output-port write
   and input-port update become visible before the consumer read.

Consequently, when a producer completes at tick `t` and the consumer starts at
tick `t`, the consumer reads the new version. A consumer that started before
`t` keeps its previous snapshot and is not modified during execution.

This ordering can fit the current phase direction:

```text
ExecutionCompletion and output commit
    -> zero-delay data availability
    -> JobRelease
    -> Scheduling and job-start input read
```

It does not reuse `EventSequence` as a time or microstep. The channel
implementation task must define the precise publication/availability event
contract before allowing zero delay, because T11 currently sends in the final
`CausedAction` phase and deliberately requires positive timing.

### Compatibility with T11

T11 remains unchanged for MATLAB/Bosch conformance. Its fixed-delay route is
the first specialized communication behavior, not the final channel API.

A later implementation may introduce a new configuration schema and translate
each schema-v4 `MessageRouteSpec` into one fixed-delay channel. Existing T11
traces must retain their finish/send/delivery meaning during that migration.

## Consequences

Positive:

- user input is the single source of truth for logical connectivity;
- task/resource allocation remains independent of dataflow topology;
- one connection per directed pair prevents ambiguous duplicate paths;
- one input and output endpoint per channel makes every data dependency
  explicit;
- persistent channels and individual transfers have distinct ownership;
- zero-delay visibility has an explicit deterministic write-before-read rule;
  and
- job input snapshots make mid-execution data changes impossible.

Limiting or unresolved:

- no channel, port, `ChannelId`, payload, or `DataVersion` type is implemented
  yet;
- task-local port identifier and configuration syntax are undecided;
- initial input-port values before the first delivery are undecided;
- the representation of several named values on one channel is undecided;
- the policy for several writes to one channel at the same tick is undecided;
- zero-delay cycles must be rejected or assigned explicit fixed-point/
  microstep semantics before they are supported;
- whether every future task type reads strictly at job start requires Bosch
  and functional-model validation; and
- configuration schema evolution is deferred to the implementation task.

## Alternatives considered

### Infer channels from resource allocation

Rejected because execution placement and logical data dependency are separate
experiment decisions. Reallocation must not silently change task connectivity.

### Allow several channels for the same directed pair

Rejected for the planned baseline because it makes endpoint lookup and
read/write ownership ambiguous. Later data fields can share the single
connection.

### Share one port among several channels

Rejected because hidden fan-in or fan-out makes it harder to identify exactly
which dependency was written or read. The planned baseline gives every
channel its own source output port and destination input port.

### Treat a message as the connection

Rejected because a runtime message has one producer job and finite lifecycle,
while connectivity and delay persist across all jobs in the experiment.

### Let equal-tick insertion order decide read versus write

Rejected because insertion order is an event identity/tie-breaker, not the
semantic contract for data visibility.

## Required validation before implementation is accepted

- configuration rejects duplicate directed endpoint pairs and unknown tasks;
- reverse directions remain independently configurable;
- every channel has exactly one source output port and destination input port;
- no port is shared by several channels;
- each task's input/output port counts match its incoming/outgoing channels;
- zero-delay completion writes are visible to same-tick job starts;
- already-running jobs retain their earlier input snapshots;
- declaration order does not change channel behavior;
- zero-delay cycles and multiple same-tick writes follow an explicit policy;
- schema-v4 routes translate without changing T11 Bosch timing; and
- repeated runs produce byte-identical channel/data-version traces.
