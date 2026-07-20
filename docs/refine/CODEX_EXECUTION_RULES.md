# Codex Execution Rules

Use these rules for every task under `docs/refine/`.

## 1. Read only what is needed

Before implementation:

1. Read the selected refinement file.
2. Read only the source files explicitly listed in that file.
3. Read directly related headers and tests only when required.
4. Do not scan the whole repository unless the selected task cannot be understood otherwise.
5. Do not reread large architecture documents already summarized by the selected refinement file.

## 2. Implement one bounded task per session

Do not implement an entire goal in one session.

Use a task identifier such as:

```text
G3.2 — Canonical event table
```

Complete that task, its tests, and its local documentation before moving to another task.

## 3. Preserve architecture boundaries

- The simulator core must not depend on Dear ImGui.
- GUI code may read detached snapshots and submit commands only through existing boundaries.
- Bosch-specific GUI construction belongs in an application/adapter layer.
- Workspace preferences must not affect simulation semantics.
- File dialogs and path handling must not be implemented inside the simulator core.
- Derived plots and exports must not modify canonical traces.

## 4. Avoid broad changes

Do not:

- rename unrelated types;
- reformat unrelated files;
- redesign CMake or Make commands;
- change event ordering;
- change Bosch numerical behavior;
- update every document after a small task;
- add large dependencies without explicit approval.

## 5. Validation

For each task:

1. Build only the affected targets first.
2. Run focused tests for the affected module.
3. Run `make test` only after focused tests pass.
4. Report exact commands and results.
5. Confirm that canonical Bosch conformance remains unchanged when the task touches runtime/session wiring.

## 6. Handoff format

At the end of a task, report:

```text
Task:
Files changed:
Behavior added:
Tests added/updated:
Commands run:
Known limitations:
Suggested next task:
```

Keep the handoff factual and brief.
