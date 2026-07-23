# Glossary and Current Limits

## Glossary

| Term | Meaning in CPSSim |
|---|---|
| Tick | canonical signed integer time unit |
| Tick period | physical duration represented by one tick |
| Task | immutable periodic workload description |
| Job | one released runtime instance of a task |
| Resource | exclusive execution processor |
| Execution profile | task accessibility and deterministic demand on one resource |
| Assignment | selected resource for a task in a run |
| Run plan | stop tick, policy kind, and task assignments |
| Ready | released job waiting or waiting again after preemption |
| Running | job currently receiving execution |
| Preemption | stopping an incomplete Running job for a higher-ranked Ready job |
| Deadline miss | observation that a job is incomplete at its absolute deadline |
| Event | immutable canonical runtime observation |
| Event phase | semantic same-tick processing stage |
| Event sequence | stable insertion-order identity, not time |
| Event queue | pending event candidates |
| Canonical trace | accepted processed observations |
| Logical link | directed structural dependency with no network events |
| Communication link | directed completion-triggered fixed-delay message route |
| Functional model | optional model driven by timing actions that returns observations |
| Functional observation | typed signal values at one tick |
| Project | persistent system, run plan, workspace, and scenario metadata |
| System draft | detached editable system values |
| Snapshot | detached copy rendered by the GUI |
| Conformance | comparison with an independent captured/reference behavior |

## Current semantic limits

### Tasks

- periodic activation only;
- deterministic execution demand;
- overlapping active jobs from the same task are rejected;
- deadline misses are observed, not automatic cancellation;
- no sporadic, event-triggered, or channel-triggered task model yet.

### Resources and scheduling

- independent exclusive uniprocessors;
- one scheduling domain;
- fixed-priority policy available in current run plans;
- no global scheduling, migration, servers, fractional capacity, or accelerators;
- no modeled context-switch/preemption overhead.

### Communication

- directed ordered task pair;
- completion-triggered messages;
- fixed one-tick send offset;
- fixed configured delay;
- no payload data, port values, bandwidth, queues, contention, loss, duplication,
  or stochastic delay.

### Functional/co-simulation

- generic typed observation interface;
- one attached functional model per engine;
- prepared FMI 2.0 Co-Simulation shared library;
- synchronous stepping;
- no archive extraction, event-mode iteration, rollback, asynchronous pending
  steps, or FMI 3 support.

### GUI

- Qt Widgets is the default frontend;
- Dear ImGui remains a legacy compatibility target;
- visualization is detached from runtime state;
- no semantic effect from theme, panel layout, zoom, or frame rate;
- no current general multi-run comparison or parameter-sweep manager.

### Platform scope

The primary documented environment is Ubuntu/Linux. Other platforms require
explicit build, GUI, and FMU validation before being claimed as supported.

## Reading status labels

- **Implemented:** available and tested in the audited revision.
- **Limited:** implemented with the stated boundary.
- **Planned:** design direction recorded but not yet implemented.
- **Candidate:** valuable idea whose semantics remain undecided.
- **Deferred:** add only when a concrete model requires it.
