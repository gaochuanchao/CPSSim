# Architecture

## 1. Target architecture

CPSSim uses a hybrid co-simulation architecture:

- a discrete-event timing kernel advances between relevant events;
- functional and physical models retain their own internal stepping semantics;
- a co-simulation orchestrator coordinates both layers;
- adapters isolate Bosch, FMI, Simulink, GUI, and future backends.

```text
ExperimentRunner
    |
    v
SimulationEngine
    +-- EventQueue
    +-- ReleaseModel
    +-- ResourceAllocator
    +-- Scheduler
        +-- SchedulingPolicy
        +-- Resource 1..N
    +-- NetworkModel
    +-- FunctionalModelPort
    +-- TraceLogger
```

External integrations:

```text
FunctionalModelPort
    +-- MockFunctionalAdapter
    +-- BoschFmi2Adapter
    +-- BoschOfflineTraceAdapter
    +-- Future Python/MIMOS/mecRT adapters

VisualizationPort
    +-- DearImGuiFrontend
```

The learning-oriented
[module-interaction diagrams](../MODULE-INTERACTIONS.md) expand this overview
into the current initialization and runtime call flow, including the separate
`ResourceAllocator`, runtime `Scheduler`, `SchedulingPolicy`, `Resource`, and
engine boundaries.

## 2. Layer responsibilities

### Model layer

Defines portable records and identifiers:

- `TaskSpec`
- `TaskResourceProfile`
- `Task`
- `JobState`
- `ResourceSpec`
- `Resource`
- `Message`
- `DataVersion`
- `Event`
- `Observation`
- `ExperimentConfig`

This layer contains no GUI, FMI, or Simulink types.

### Kernel layer

Owns:

- current logical tick;
- event queue;
- same-time event ordering;
- runtime scheduler coordination;
- interaction with allocation, scheduling, and network policies;
- canonical event generation; and
- task-driven periodic-release progression.

### Policy layer

Computes placement and runtime scheduling decisions without mutating kernel
state.

A resource allocator receives immutable experiment configuration and returns a
complete task-to-resource assignment plan. It may later use configured records
or an allocation algorithm, but it does not parse files or mutate runtime
tasks directly.

A runtime scheduling policy receives read-only resource and job views. It may
choose:

- which ready job to dispatch;
- whether a running job should be preempted;
- dynamic priority values;
- later, cancellation or degradation decisions.

Neither policy interface mutates tasks or resources directly. The kernel's
runtime `Scheduler` owns released jobs, per-resource Ready queues, and runtime
resources. It validates and applies `SchedulingPolicy` decisions through
public `Resource` transition methods. `SimulationEngine` owns the global event
cycle and invokes this scheduling domain.

`ExperimentConfig` owns one experiment-wide `SchedulingSpec`. Its preemption
mode is visible to allocation analysis, the runtime scheduler that enforces
it, and the engine that exposes the run's assumption. It is not duplicated in
every task or job.

### Network layer

The current `FixedDelayNetwork` owns generic task routes, runtime message IDs,
and message lifecycle (`PendingSend`, `InFlight`, `Delivered`). The global
`EventQueue`, rather than the network, owns pending send and delivery
candidates.

An accepted source-job completion publishes one message per matching route.
The network produces causally linked `MessageSend` and `MessageDelivery`
events but does not release receiver jobs or directly call functional models.
Task endpoints deliberately do not imply resources, servers, uplink/downlink
labels, or Bosch triggers. See the
[causal-message module](../modules/causal-messages.md) and
[ADR-0010](../adr/0010-generate-task-routed-messages-with-fixed-delay.md).

The proposed future dataflow model is recorded separately in
[ADR-0011](../adr/0011-plan-user-configured-task-channels.md). It introduces
one persistent single-directional, user-configured channel per directed task
pair, distinct from individual messages/data versions. Every channel has its
own source-task output port and destination-task input port. For a zero-delay
channel, successful completion writes precede same-tick job-start reads. This
is a planned contract, not behavior implemented by the current positive-delay
network.

### Functional interface

Defines a simulator-independent port:

```text
initialize(config)
advanceTo(tick, environmentInput)
applyActions(actionBatch)
observe(request)
finalize()
```

The current port is implemented as `FunctionalModel` plus `FunctionalRuntime`.
Observation `t` is available before scheduling at `t`; actions accepted at
`t` affect interval `[t, t + 1)` and first appear in observation `t + 1`.
The runtime validates and owns the typed append-only functional trace. See
[ADR-0017](../adr/0017-order-online-functional-observation-before-same-tick-actions.md)
and the
[online-functional module](../modules/online-functional-interaction.md).

### Adapter layer

Translates generic actions and observations into backend-specific APIs.

Examples:

- generic action to Bosch Boolean trigger;
- generic advance call to FMI `doStep`;
- trace records to Simulink `timeseries`;
- simulator snapshots to Dear ImGui panels.

The generic, core-independent
[`Fmi2CoSimulation`](../../src/cpssim/fmi/fmi2_importer.hpp) adapter loads a
prepared FMI 2.0 Co-Simulation platform library. It owns library loading,
component lifecycle, typed value-reference access, `doStep`, status
propagation, and cleanup. Online functional interaction connects it through a
generic core interface; the Bosch-specific adapter owns trajectory, parameter,
trigger, and output mappings. See the
[FMI module](../modules/fmi2-co-simulation.md) and
[ADR-0016](../adr/0016-use-a-small-fmi2-co-simulation-loader.md).

### Application-interface layer

Application services compose existing core and adapter APIs for user
interfaces without becoming part of the generic core. The first service is
`DefaultBoschRunService`: it consumes a typed `BoschRunRequest`, loads one
supplied trajectory, constructs the Bosch FMI functional model, invokes the
existing reference-scenario runner, and returns detached summary data.

The CLI registry converts either direct argv or interactive wizard choices to
the same request and injects the service for testing. Terminal prompts contain
no simulation semantics. The internal Bosch compatibility executable calls
the same service. See the
[application-interface module](../modules/application-interfaces.md) and
[ADR-0022](../adr/0022-share-application-services-across-cli-paths.md).

### GUI layer

The GUI uses a one-way presentation boundary. `SimulationController` copies
public engine state into detached `SimulationSnapshot` values and accepts FIFO
commands. The renderer consumes those copies and sends:

- run;
- pause;
- reset;
- step to next event;

One controller update processes at most one complete logical event tick while
running. `SimulationEngine::run()` and GUI stepping use the same event-phase
implementation. The GUI must not inspect live mutable kernel containers, and
Dear ImGui/GLFW/OpenGL types remain outside `cpssim_core`.

The current workbench adds an explicit draft/active run-plan boundary, shared
strong-ID selection, a derived architecture graph and scheduling timeline, and
typed functional plots without weakening that dependency direction.
Step-one-physical-tick, playback pacing, workspace persistence, and background
simulation remain future work. See
[ADR-0018](../adr/0018-use-a-single-threaded-snapshot-command-gui-boundary.md),
the [GUI tutorial](../gui/README.md), and the
[GUI architecture guide](../gui/GUI_ARCHITECTURE.md).

## 3. State ownership

| State | Owner |
|---|---|
| Current tick | `SimulationEngine` |
| Pending events | `EventQueue` |
| Resource-allocation rule | read-only `ResourceAllocator` |
| Applied task assignment and per-task release position | runtime `Task` |
| Runtime task collection | `PeriodicReleaseModel` |
| Released jobs and per-resource Ready membership | runtime `Scheduler` |
| Job lifecycle, timing outcomes, and remaining execution | scheduler job store |
| Runtime job-selection contract | read-only `SchedulingPolicy` |
| Fixed-priority selection rule | `FixedPriorityPolicy` |
| Running identity, interval, completion expectation, and busy time | each runtime `Resource` |
| Preemption assumption | immutable `SchedulingSpec`, enforced by `Scheduler` |
| Message routes, IDs, planned timing, and lifecycle | `FixedDelayNetwork` |
| Plant and controller state | functional-model adapter |
| FMI library handle, function table, component, and lifecycle | `Fmi2CoSimulation` |
| Immutable event history | trace logger |
| Bosch trigger mapping | Bosch adapter |
| Rendered state | GUI snapshot |
| Pending presentation commands | `GuiCommandQueue` |
| CLI command collection and dispatch | `CommandRegistry` / `CliApplication` |
| One supplied Bosch run sequence | `DefaultBoschRunService` |

The portable runtime records and their coordinated state changes are defined
in the [runtime-state module](../modules/runtime-state.md). The records enforce
execution/lifecycle consistency. The runtime `Scheduler` owns their canonical
job collection and Ready membership, while each initial `Resource` owns at
most one Running execution interval. Spatial or fractional allocation remains
a future resource model rather than an unused abstraction.

Task configuration does not select a resource. `TaskResourceProfile` records
accessible resources and execution time for each task-resource pair. A
`ResourceAllocator` returns a complete placement plan; the engine validates and
applies it to runtime `Task` objects. A generated job captures that assignment
and demand. Reassignment affects future jobs, not pending or released jobs.
[ADR-0006](../adr/0006-assign-resources-to-runtime-tasks.md) records this
ownership boundary.

## 4. Time model

Canonical time is an integer tick:

```cpp
using Tick = std::int64_t;
```

A configuration defines the physical duration of one tick. For the current
Bosch model, the intended initial quantum is `1e-4` seconds.

Floating-point seconds may be used only for display, file interchange when
required, or external APIs. Conversion must be explicit and validated.

ADR-0001 fixes the initial conversion boundary to exact integer nanoseconds
per tick and rejects negative, inexact, or overflowing conversions.

## 5. Event identity and ordering

The portable record and deterministic JSON line are defined in the
[canonical-event module](../modules/canonical-events.md). The pending-event
owner and ordering are defined in the
[event-queue module](../modules/event-queue.md). Every event has:

- logical tick;
- event phase;
- monotonically allocated insertion sequence;
- event type;
- optional causal predecessor;
- stable entity identifiers.

The accepted same-tick phase precedence is:

1. `ExecutionCompletion` — completion and publication;
2. `MessageDelivery` — message delivery;
3. `DeadlineCheck` — deadline and overrun handling;
4. `JobRelease` — job release;
5. `PolicyUpdate` — policy-state update;
6. `Scheduling` — preemption and dispatch;
7. `CausedAction` — caused sends and downstream events;

The queue compares smaller tick first, this explicit phase precedence second,
and smaller insertion sequence third. Enum declaration order is not canonical
queue precedence. [ADR-0004](../adr/0004-order-events-by-tick-phase-and-sequence.md)
records the ordering, zero-based allocation, and decision to defer superdense
time until a concrete event-iteration requirement exists.

When the engine reaches `JobRelease`, it processes all periodic releases at
that tick as a task-domain batch ordered by priority and `TaskId`. This does
not change the queue comparator or rewrite insertion identity; it makes
runtime-generated successors reproduce simultaneous task-release semantics.
[ADR-0013](../adr/0013-order-same-tick-periodic-releases-by-task-semantics.md)
records the refinement found during Bosch timing conformance.

The event queue does not process events, enforce a current engine tick,
implement same-time fixed-point iteration, cancel events, or own an append-only
trace collection.

Runtime tasks, assignment, and periodic production are defined in the
[periodic-release module](../modules/periodic-releases.md). Its runtime model
schedules at most one release per task. `Task::release` creates the concrete
job and advances that task by one period only after the engine processes the
current release. `JobId` is a one-based
task-local number; runtime collections use the complete `(TaskId, JobId)`
`JobIdentity`. Queue-owned `EventSequence` remains a separate zero-based
insertion identity. [ADR-0005](../adr/0005-schedule-periodic-releases-at-runtime.md)
records these semantics and separates pending events from runtime history.

The complete scheduling cycle is defined in the
[fixed-priority scheduling module](../modules/fixed-priority-scheduling.md),
with `SchedulingPolicy` as the read-only policy interface and
`FixedPriorityPolicy` as the first implementation. `ResourceAllocator` chooses
initial task placement. Execution is charged over event intervals, obsolete
completion candidates are ignored by
expected-tick matching, and completion/deadline/release/scheduling follow the
established event phases.
[ADR-0007](../adr/0007-separate-fixed-priority-policy-from-event-driven-mechanism.md)
records the policy boundary and timing semantics; its current ownership
refinement is [ADR-0009](../adr/0009-separate-scheduler-policy-resource-and-engine-ownership.md).

Multiple independent resources are described in the
[multiple-resources module](../modules/multiple-resources.md). The engine owns
one global event timeline, release model, and trace. One runtime `Scheduler`
owns all released jobs, one Ready queue per resource, and all runtime
`Resource` objects. Each resource owns its active execution interval and
integer busy-time accounting. The scheduler invokes the read-only policy and
visits resources in ascending `ResourceId`. Jobs never enter another
resource's Ready queue.
[ADR-0008](../adr/0008-order-independent-resources-by-resource-id.md) records
the deterministic resource order, while
[ADR-0009](../adr/0009-separate-scheduler-policy-resource-and-engine-ownership.md)
records the current ownership and experiment-wide preemption setting.

The first generic communication path is defined in the
[causal-message module](../modules/causal-messages.md). Only completions
accepted by the runtime scheduler publish. `FixedDelayNetwork` expands routes
in canonical task-ID order, owns one-based runtime message identities, and
advances messages from `PendingSend` through `InFlight` to `Delivered`.
`SimulationEngine` routes the accepted completion, send, and delivery events
and appends successful observations; it does not mutate message state itself.
The causal chain is `JobFinish -> MessageSend -> MessageDelivery`, with each
effect referring to its direct predecessor sequence. Positive integer send
offset and delay avoid same-tick fixed-point iteration.

## 6. Portable data design

Prefer plain records with stable fields.

All project C++ APIs use the single `cpssim` namespace. Source directories
such as `model/`, `config/`, and `kernel/` express architecture and file
ownership but do not create nested public namespaces. Public type names must
therefore remain unique within CPSSim.

Do not use these in core public interfaces:

- MATLAB tables;
- MATLAB `timeseries`;
- Simulink block handles;
- FMI variable references;
- Dear ImGui types;
- raw pointers to mutable internal objects.

JSON is the initial configuration format. Its strict versioned schema is
documented in the
[experiment-configuration module](../modules/experiment-configuration.md) and
[ADR-0002](../adr/0002-use-versioned-json-configuration.md). Canonical events use
versioned JSON Lines as their event-record representation, as recorded in
[ADR-0003](../adr/0003-use-versioned-json-lines-event-records.md). CSV remains
a derived analysis projection.

Schema version 4 adds generic `message_routes`. Versions 1–3 translate to no
routes, so loading an old experiment does not silently enable communication.

## 7. Dependency rule

Dependencies point inward toward the core.

The core is not allowed to include headers from:

- Dear ImGui;
- ImPlot;
- SDL or GLFW;
- FMI importer libraries;
- MATLAB or Simulink;
- Bosch-specific adapters.

## 8. Initial repository structure

```text
CMakeLists.txt
cmake/
config/
docs/
    guide/
    modules/
    instructions/
    adr/
experiments/
resources/
src/
    cpssim/
        application/
        core/
        model/
        kernel/
        policy/
        network/
        functional/
apps/
    cli/
    gui/
adapters/
    bosch/
    fmi2/
tests/
    unit/
    integration/
    conformance/
tools/
```

The structure may evolve, but major changes require an ADR.
