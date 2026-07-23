# ADR-0017: Observe Before Applying Same-Tick Functional Actions

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T13, T15, T16

## Context

T16 connects the integer-tick event engine to replaceable functional models.
The generic boundary must support a deterministic mock and the Bosch FMI 2.0
Co-Simulation model without placing FMI value references or Bosch trigger
names in `cpssim_core`.

The coupling order is observable behavior. At tick `t`, CPSSim may have a
physical observation, completion/delivery/release events, scheduling choices,
and Bosch trigger pulses. Choosing whether the actions at `t` affect the state
reported at `t` or the following physical interval changes both policy input
and the captured FMU output.

The supplied Simulink model uses a 0.0001-second discrete input and FMU sample
time. Its output trace contains the initialized state at time zero. Inputs and
trigger pulses sampled at tick `t` are then supplied before the FMI step from
`t` to `t + 1`, and the resulting state is observed at `t + 1`.

This ordering was checked against the first nontrivial captured transition:
`actuator_finished` occurs at tick 305 and the resulting actuator command is
visible in output row 306. An initial apparent ordering discrepancy was traced
instead to missing FMI parameter initialization. The Bosch C implementation
zero-initializes controller-gain storage, so the importing master must apply
the start values declared by `modelDescription.xml` before leaving
initialization mode, as the supplied Simulink block does.

T16 must also prove that a canonical event trace can reproduce the online
functional trace without launching MATLAB or Simulink. Online and replay paths
therefore need one explicit contract rather than two similar event loops.

## Decision

### Generic boundary

`cpssim_core` defines a runtime-polymorphic `FunctionalModel` interface using
only CPSSim types:

- initialize with a positive integer tick period and inclusive stop tick;
- advance to a nondecreasing integer target tick;
- apply one ordered batch of accepted canonical `Event` values at the current
  tick; and
- finalize the external model.

Each initialization or advance returns typed, named observations. Real,
Integer, and Boolean signals remain distinct rather than being flattened into
one untyped numeric vector. Signal order is stable and part of deterministic
trace comparison, while signal names let policies and adapters select values
without positional casts.

A `FunctionalRuntime` coordinates this interface and owns the append-only
functional observation trace. It validates lifecycle, nondecreasing time,
consecutive observation ticks, action tick consistency, the inclusive stop
tick, and at most one action batch per visited tick. It does not own the
functional model object.

### Online tick order

The accepted cycle is:

```text
initialize functional model
record observation at tick 0
forward observation to scheduling policy

for each canonical event tick t:
    advance functional model from its current tick to t
    record every intermediate integer-tick observation
    forward observations to the scheduling policy in tick order
    process completion/delivery/deadline/release phases at t
    run scheduling at t
    finish caused actions at t
    apply the accepted canonical event batch to the functional model at t

advance functional model to the inclusive stop tick
record and forward remaining observations
finalize functional model
```

Thus an observation at tick `t` is available before the scheduling decision at
`t`. Actions accepted at `t` cannot retroactively change that observation;
they affect the physical interval `[t, t + 1)` and become visible in the
observation at `t + 1`.

When the event queue has no event at an intermediate physical tick, the
functional model still advances and produces its observation. The discrete
event engine does not insert artificial queue events for those steps; the
adapter advances its own model between event ticks.

Events accepted at the inclusive stop tick are retained in the canonical
trace and are offered as the final action batch, but there is no physical step
beyond the configured horizon in T16.

### Environment inputs

The generic engine does not interpret trajectory columns. A functional-model
implementation owns or references its immutable environment-input sequence.
The Bosch implementation receives validated feedforward and velocity samples
at construction and applies sample `t` before stepping `[t, t + 1)`.

This keeps trajectory schema and FMI value references outside the core while
allowing a future adapter to obtain inputs from another deterministic source.

### Policy observations

`SchedulingPolicy` gains a default no-op observation hook. `Scheduler`
forwards each read-only `FunctionalObservation` supplied by the engine. A
policy may update its own private decision state, but it still cannot mutate
jobs, resources, the event queue, time, or functional-model state.

T16 validates this boundary with one minimal observation-aware test policy.
`FixedPriorityPolicy` keeps its existing behavior through the default no-op
hook. General context-aware algorithms, observation histories, and policy
action schemas are later work.

### Bosch FMI translation

`BoschFmi2FunctionalModel` combines the generic functional interface, the T13
trigger projection, and the T15 FMI importer in a separate adapter target. It:

- sets the v10 initial state parameters during FMI initialization;
- sets all v10 controller, initial-state, noise, and initial-velocity
  parameters declared by the supplied Simulink block;
- maps accepted event batches to the sixteen explicit Boolean value
  references;
- applies feedforward and velocity inputs at each step;
- derives every FMI communication point and step size from integer ticks;
- resets trigger pulses after their one-tick interval;
- reads the six captured output signals after each step; and
- converts unsuccessful FMI calls into explicit runtime failures.

The core does not depend on this adapter.

### Offline replay equivalence

Offline replay groups a completed canonical event trace by integer tick and
feeds those batches through the same `FunctionalRuntime` order. The trace must
already be nondecreasing and all events must lie inside the horizon.

For the deterministic mock and Bosch adapter, online and offline functional
observations must compare exactly within one build. Comparison with the
captured Simulink/FMU CSV uses exact tick, Integer, and Boolean equality plus
an explicitly documented absolute/relative tolerance for Real values.

## Consequences

Positive:

- functional observations can influence a later same-tick scheduling choice;
- canonical event time remains integer and FMI seconds remain an adapter
  boundary;
- event-driven scheduling does not need artificial per-physical-tick events;
- online and replay paths have one lifecycle and ordering validator;
- Bosch, FMI, trajectory, and selected-output details stay outside the core;
  and
- the functional trace is deterministic, typed, named, and append-only.

Limiting:

- T16 samples one functional observation at every integer tick, which is
  appropriate for the v10 reference but may be configurable in a later model;
- the Bosch trajectory is held as immutable adapter input rather than streamed;
- events at the stop tick do not affect an out-of-horizon physical step;
- the policy hook receives observations but no rich context-query or action
  language is introduced; and
- FMI event mode, superdense iterations, rollback, and asynchronous `doStep`
  remain unsupported.

## Alternatives considered

### Apply actions before observing at the same tick

Rejected because it would make a trigger at `t` alter the state labeled `t`,
contrary to the initialized time-zero row and sampled-step behavior of the
reference model.

### Add every physical step to `EventQueue`

Rejected because it converts the discrete-event kernel into a tick-driven
queue and creates 150,001 artificial events for the current reference. The
functional adapter can advance through quiet physical ticks independently.

### Let the Bosch adapter modify scheduling state directly

Rejected because external-model translation and scheduling mechanism are
separate owners. Observations enter through the policy interface; only
`Scheduler` applies policy decisions.

### Store all functional values as `double`

Rejected because it loses the distinction between physical values, counters,
and Boolean states and would weaken exact conformance checks.

## Required validation

- runtime lifecycle and tick-order violations are rejected;
- intermediate observations are consecutive even across large event jumps;
- actions at tick `t` first affect observation `t + 1`;
- observations reach a test policy before same-tick selection;
- fixed-priority timing traces remain unchanged without a functional model;
- mock online and offline traces are exactly equal;
- both Bosch mappings produce exact online/offline traces;
- both Bosch output CSVs match under the recorded tolerance policy; and
- Debug, Release, Clang, clang-tidy, sanitizer, timing-conformance, and
  checksum validation continue to pass.
