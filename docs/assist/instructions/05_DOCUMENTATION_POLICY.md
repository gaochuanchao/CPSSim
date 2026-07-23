# Documentation Policy

## 1. Purpose

Documentation must preserve the reasoning and implementation details needed to:

- inspect simulator semantics;
- reproduce experiments;
- understand why an interface exists;
- modify the implementation safely;
- compare later implementations with the original design.

Chat history is not the project record. Relevant conclusions must be written
into the repository.

The reader-facing entry point is `docs/README.md`. Connected explanations
belong under `docs/guide/`; focused ownership contracts belong under
`docs/modules/`. Prefer a short diagram, table, or worked example when it makes
a relationship clearer than continuous prose. Documentation describes the
current project; Git history preserves chronology.

## 2. Required documentation types

### Architecture documents

Describe stable subsystem boundaries and ownership.

Update when:

- a module is added or removed;
- ownership of state changes;
- a dependency direction changes;
- an external adapter is introduced.

### Architecture Decision Records

Use an ADR for decisions that are difficult to reverse or affect semantics.

Examples:

- integer time quantum;
- same-time event ordering;
- event cancellation strategy;
- scheduling-policy interface;
- functional-model call ordering;
- FMI importer choice;
- GUI threading model;
- trace schema.

### Current learning guides

Use `docs/guide/` for explanations that connect several modules or teach a
workflow. A guide should state why the concept matters, show relationships,
link to current source and tests, distinguish everyday knowledge from advanced
details, and remain synchronized with current code. Add small explanations of
unfamiliar tools or C++ constructs to the developer guide or relevant module
page rather than creating a task-numbered diary.

Do not assume a command name or tool is self-explanatory. Relate it to
reproducibility, traceability, validation, portability, or maintainability.

### Design semantics and cross-references

A declaration is not a complete design explanation. When introducing a state,
event, command, policy decision, or other behavior-bearing category, document
the following where applicable:

1. the condition represented by each value;
2. what causes entry, exit, or emission;
3. preconditions and resulting state changes;
4. terminal states and forbidden transitions;
5. which module owns and enforces the behavior;
6. relevant failure or rejection behavior;
7. observable events or trace records; and
8. unresolved semantics that a later task must decide.

Clearly distinguish among:

- behavior implemented and tested now;
- an intended contract for a later implementation task; and
- an open design question that must not be inferred from a name.

Use relative Markdown links near the claims they support. Prefer links to the
actual source declaration, behavior tests, module contract, ADR, architecture
section, and future-work proposal over unlinked path text. A reader should be able
to move from a design explanation to its implementation and validation
without searching the repository manually.

Do not overstate test coverage. If tests currently verify only type properties
but not runtime transitions, say so explicitly and link to the future-work
proposal that owns the missing behavior.

### Module documentation

Each module should document:

- responsibility;
- public interface;
- owned state;
- invariants;
- dependencies;
- failure behavior;
- examples where useful.

### Experiment documentation

Each experiment directory should contain:

- purpose;
- configuration;
- expected outputs;
- seed;
- command;
- result summary;
- known limitations.

### Test documentation

Non-obvious conformance and golden tests should explain:

- source of expected behavior;
- what is compared;
- tolerance;
- regeneration procedure.

## 3. Code comments

Comments should explain:

- why a decision is necessary;
- semantic invariants;
- externally imposed constraints;
- non-obvious numerical or ordering behavior.

Do not use comments to restate straightforward code.

Every project-owned C++ header and implementation file begins with the source
documentation block defined in the
[C++ style rules](06_CXX_STYLE_AND_DESIGN_RULES.md#11-source-file-and-function-descriptions).
Every function also has a nearby contract or implementation description. Use
plain `//` for a short one-line explanation inside a class or function, and
use `/*** ... ***/` for multi-line descriptions and descriptions before a
class or top-level function. Include parameters, results, failure behavior,
state changes, or invariants when they help a reader use or modify the function
safely.

When a declaration and definition are separate, their comments have different
jobs: the header explains what callers can rely on, while the implementation
explains validation and non-obvious state changes. Do not copy a long comment
word-for-word between the two locations.

## 4. Documentation update checklist

Before completing a task, ask:

- Did the public interface change?
- Did simulation semantics change?
- Did state ownership change?
- Was a new limitation introduced?
- Was a new test fixture created?
- Can a new developer understand how to reproduce the result?
- Can a developer understand why each newly introduced tool or file exists?

If yes, update the relevant documentation.

## 5. Documentation quality rule

Avoid vague statements such as:

- “implemented scheduler”;
- “fixed timing”;
- “added tests.”

Instead write concrete statements, for example:

- “Added fixed-priority preemption when a newly released ready job has a
  numerically lower priority value than the currently running job.”
- “Remaining execution is charged to the running job before processing events
  at the next event tick.”
- “Tie-breaking uses task ID followed by job sequence number.”
