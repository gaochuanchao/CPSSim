# Development Workflow

## 1. Branch and task discipline

Use one focused branch per task or coherent change.

A task should normally change one subsystem and be reviewable without relying
on unrelated future work.

Suggested branch names:

```text
feature/event-model
feature/fp-scheduler
test/matlab-golden-trace
docs/adr-same-time-ordering
fix/job-completion-order
```

## 2. Before coding

For each task:

1. Read the relevant architecture and ADR documents.
2. Define the observable behavior.
3. Identify the owner of mutable state.
4. Identify test cases.
5. Check whether the task changes a public interface or simulation semantics.
6. Create an ADR first when required.

## 3. During coding

- Keep changes small.
- Prefer explicit code over highly generic abstractions.
- Add assertions for internal invariants.
- Do not add GUI or FMI dependencies to the core.
- Do not optimize before measurement.
- Avoid changing multiple semantic dimensions at once.

Examples of changes that should not be combined:

- replacing the event queue and changing same-time ordering;
- introducing multicore scheduling and stochastic execution times;
- adding FMI integration and changing scheduler semantics.

## 4. After coding

Run:

- formatter;
- compiler with warnings enabled;
- unit tests;
- relevant integration tests;
- sanitizer build when applicable;
- conformance comparison when timing behavior changed.

Then update:

- module documentation;
- the relevant current guide when a change connects several modules or tools;
- tests and expected behavior;
- `docs/guide/AGENT-HANDOFF.md` when the implemented project boundary changes;
- ADR status if the implementation completes a decision.

## 5. Suggested local commands

Use `docs/COMMANDS.md` as the quick reference for normal Debug, Release,
sanitizer, formatting, analysis, and reference-validation workflows.

The direct development commands are:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Sanitizer build:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

## 6. Commit content

A complete implementation commit should normally contain:

- source changes;
- tests;
- current documentation.

Avoid commits that say only “update code” or “fix issue.”

Suggested format:

```text
kernel: add deterministic same-tick event ordering

- introduce event phase and insertion sequence
- add ordering unit tests
- document ordering invariant
- record ADR-0002 implementation status
```

## 7. Codex interaction protocol

When asking Codex to implement a task, provide:

- the exact requested scope, roadmap phase, or future-work proposal;
- files it should read;
- constraints that must remain unchanged;
- expected tests;
- expected documentation updates;
- explicit instruction not to implement later tasks.

Example:

```text
Implement only the first approved stage of F1: run a configured headless
experiment and write canonical JSON Lines. Do not add analysis reports yet.

Read AGENTS.md, docs/instructions/01_ARCHITECTURE.md,
docs/instructions/04_TESTING_AND_VALIDATION.md,
docs/guide/FUTURE-WORK.md, and the relevant module pages and tests.

Before editing, report state ownership, files, tests, documentation, and open
decisions. Preserve deterministic integer-tick behavior. Update current module
and guide documentation, show the diff, and do not commit unless requested.
```
