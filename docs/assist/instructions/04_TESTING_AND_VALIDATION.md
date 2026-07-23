# Testing and Validation

## 1. Testing levels

### Unit tests

Test individual records, queues, lifecycle transitions, and policy decisions.

Examples:

- invalid period or execution time is rejected;
- event ordering is stable;
- a higher-priority release preempts a lower-priority job;
- remaining execution time is updated exactly once;
- a completed job cannot resume;
- a cancelled job cannot complete.

### Integration tests

Test several modules together.

Examples:

- periodic releases plus one processor;
- two resources with scheduler-owned independent Ready queues;
- causal message generation after task completion;
- deadline checks under overload.

### Conformance tests

Compare the C++ simulator with the existing MATLAB reference.

Required initial scenarios:

1. dedicated-resource mapping;
2. shared single-cloud-core mapping.

Compare:

- release ticks;
- start ticks;
- completion ticks;
- preemption and resumption ticks;
- message send and arrival ticks;
- deadline outcomes;
- Bosch trigger matrix.

### FMU replay tests

Compare:

- functional labels;
- physical state;
- performance metrics;
- violation counters;
- critical-section outputs.

Use exact equality only where justified. Numerical comparisons require explicit
absolute and relative tolerances.

## 2. Golden artifacts

Golden files must include metadata:

- simulator version or commit;
- experiment configuration;
- time quantum;
- random seeds;
- source of the reference;
- expected number of events;
- tolerance policy.

Golden files must not be silently regenerated. Regeneration requires a
documented reason and review.

## 3. Determinism tests

For a fixed configuration and seed:

- run the experiment multiple times;
- compare canonical traces byte-for-byte;
- run with and without GUI;
- run with different GUI refresh rates;
- run debug and release builds where practical.

The trace must not depend on:

- pointer addresses;
- hash-map iteration order;
- thread scheduling;
- wall-clock time;
- GUI frame timing.

## 4. Invariants

The simulator should assert or test at least these invariants:

- a job has exactly one lifecycle state;
- a running job is assigned to exactly one resource;
- a job cannot be both ready and running;
- remaining execution is never negative;
- completion occurs only when remaining execution reaches zero;
- event time never moves backward;
- sequence numbers are unique and increasing;
- messages have stable source and destination IDs;
- each processed send is caused by its accepted completion and each processed
  delivery is caused by its send;
- stale completion candidates do not publish duplicate messages;
- canonical events are append-only.

## 5. Failure reporting

A conformance comparison should report the first divergence.

Example:

```text
First mismatch:
  tick: 1250
  phase: MESSAGE_DELIVERY
  expected event: message 17 delivered to vehicle 1
  actual event: no matching event
  previous matching sequence: 381
```

Final plots alone are not sufficient validation.

## 6. Randomness

Use:

- one master experiment seed;
- separately named random streams for execution time, network delay, packet
  loss, disturbance, and policies;
- logged sampled values when they affect behavior.

Do not depend on random-number call order across unrelated modules.
