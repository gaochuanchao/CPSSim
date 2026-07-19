# Instructions for Codex and Other Coding Agents

## Project objective

Develop a portable, deterministic, event-driven C++ simulator for
physics-driven real-time cyber-physical systems. The first supported scenario
is the Bosch lateral-motion-control challenge, but the simulator must not be
hard-coded around its six tasks, one vehicle, or sixteen trigger signals.

The immediate goal is a reliable research prototype, not a feature-complete
product.

## Required reading before changing code

Use progressive context loading. Before every implementation task, read:

1. `docs/guide/AGENT-HANDOFF.md`;
2. the relevant module page and ADR;
3. the public header and closest test.

Read the charter, architecture, roadmap, workflow, testing policy,
documentation policy, or C++ rules when the requested scope touches them. The
repository keeps current documentation rather than chronological task notes;
use Git history only when historical detail is necessary. The non-negotiable
rules below always apply.

## Non-negotiable design constraints

1. Use integer simulation ticks. Do not use floating-point values as canonical
   event timestamps.
2. Keep the simulator core independent of:
   - Dear ImGui;
   - MATLAB and Simulink;
   - FMI;
   - the Bosch trigger encoding;
   - a specific scheduling policy.
3. Separate immutable specifications from mutable runtime state.
4. Keep scheduling policy separate from scheduling mechanism.
5. Use an append-only canonical event trace.
6. Define deterministic ordering for events at the same logical tick.
7. Preserve reproducibility through explicit seeds and logged random samples.
8. Do not let GUI refresh timing influence simulation behavior.
9. Do not bypass public interfaces to mutate internal simulator state.
10. Do not introduce global mutable state.

## Implementation discipline

Work in small, reviewable steps.

For every task:

1. State the intended change before editing.
2. Identify the affected module and its owner of state.
3. Add or update tests first when practical.
4. Implement the smallest complete behavior.
5. Run the relevant tests.
6. Update documentation in the same change.
7. Update `docs/guide/AGENT-HANDOFF.md` when the current project boundary changes.
8. Stop after the requested scope is complete.

Do not implement future roadmap features unless the current task explicitly
requires them.

## Documentation requirement

Every non-trivial implementation change must update at least one of:

- module documentation;
- an ADR;
- a current guide;
- test documentation;
- user-facing experiment documentation.

Implementation details must not exist only in code comments or chat history.

## Validation requirement

The current MATLAB implementation is the initial behavior oracle. The C++
implementation must eventually reproduce:

- periodic job releases;
- fixed-priority preemptive uniprocessor execution;
- release, start, finish, preemption, resumption, and deadline events;
- separate local and cloud mappings;
- causally generated uplink and downlink events;
- the dedicated-resource reference trace;
- the shared-cloud reference trace.

Do not change semantics merely to simplify the C++ implementation. If a
semantic discrepancy is found, document it explicitly and obtain a design
decision before proceeding.

## Coding style

- Use C++20.
- Prefer value types, RAII, and explicit ownership.
- Prefer `enum class` for finite event and state categories.
- Prefer strong identifier types over unstructured integers where practical.
- Avoid inheritance unless a stable runtime-polymorphic interface is needed.
- Prefer composition and small interfaces.
- Do not expose mutable containers from classes.
- Avoid premature templates and generic abstractions.
- Treat warnings as errors in CI for project code.
- Use sanitizers in debug builds when supported.

## Expected repository targets

The intended build targets are:

- `cpssim_core`: headless simulator library;
- `cpssim_cli`: command-line experiment runner;
- `cpssim_gui`: optional Dear ImGui visualization application;
- `cpssim_bosch_adapter`: Bosch-specific event and trigger mapping;
- `cpssim_fmi2_adapter`: FMI 2.0 Co-Simulation adapter;
- `cpssim_tests`: unit and conformance tests.

The exact names can change through an ADR, but dependency direction must remain:

```text
core <- cli
core <- gui
core <- Bosch adapter
core <- FMI adapter
```

The core must never depend on the adapters or GUI.

## When uncertain

Do not silently guess. Record unresolved future work in
`docs/guide/FUTURE-WORK.md` and propose one or more explicit alternatives.
Create an ADR when the decision affects architecture, public interfaces, time
semantics, determinism, or cross-platform behavior.
