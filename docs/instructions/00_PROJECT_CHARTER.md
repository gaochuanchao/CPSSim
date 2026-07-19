# Project Charter

## Project and repository name

CPSSim

CPSSim is developed as a standalone repository. It originated from the Bosch
Physics-Driven Real-Time CPS Challenge repository, whose FMU, Simulink model,
example data, documentation, authorship, and license notices remain preserved
and credited as reference material.

The `matlab` branch retains the earlier MATLAB-first development history. The
portable simulator on `main` is not expected to synchronize with Bosch's main
branch; compatibility is established through documented reference artifacts
and conformance tests instead.

## Research objective

CPSSim will provide a portable simulation foundation for studying the
interaction between real-time scheduling, communication, functional chains,
physical dynamics, and runtime performance in distributed cyber-physical
systems.

The first application is the Bosch Physics-Driven Real-Time CPS Challenge.
Later applications may include mobility models, SUMO, learning-enabled
controllers, perception and prediction models, information fusion, edge/cloud
execution, and MIMOS or mecRT integration.

## Immediate objective

Reimplement the validated timing semantics in C++ without changing their
behavior.

The immediate work is infrastructure-oriented:

- clean model representation;
- deterministic event semantics;
- scheduling-policy interfaces;
- trace and replay;
- automated tests;
- documentation.

Context-aware scheduling is deliberately postponed.

## Scope of the initial prototype

Included:

- periodic task releases;
- integer ticks;
- deterministic execution times;
- fixed-priority preemptive uniprocessor scheduling;
- multiple independent resources;
- local and cloud mappings;
- causally generated messages;
- deadline monitoring;
- canonical trace export;
- Bosch trigger encoding;
- headless execution.

Excluded initially:

- global multicore scheduling;
- migration;
- detailed cache effects;
- limited-preemption regions;
- stochastic networks;
- AI inference models;
- SUMO coupling;
- adaptive scheduling;
- polished end-user GUI.

## Success criteria

A result is considered trustworthy only if its timing semantics are:

- explicit;
- deterministic under a fixed configuration and seed;
- testable without the FMU;
- traceable through an event log;
- replayable;
- independent of GUI rendering;
- documented.

## Research-prototype principle

Prefer a small, correct, inspectable implementation over a broad but weakly
validated simulator.
