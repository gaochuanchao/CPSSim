# Documentation Maintenance

## Authoritative layers

```text
README.md          project landing page
docs/user/         current use and concepts
docs/developer/    current architecture, implementation, extension
docs/assist/       ADRs, deep notes, plans, historical evidence
Git history        chronology
tests              executable behavior evidence
```

A historical plan under `docs/assist/refine/` must not override current
reader-facing behavior.

## Chapter quality rule

Every important concept should include, where applicable:

1. representation;
2. motivation;
3. invariants;
4. numerical or workflow example;
5. runtime behavior;
6. implementation owner;
7. declaration and definition links;
8. closest tests;
9. extension guidance;
10. current limits.

## Link rule

Prefer relative links so documentation follows the checked-out branch:

```markdown
[`SimulationEngine`](../../src/cpssim/kernel/simulation_engine.hpp)
```

Use commit-pinned GitHub links only for audit evidence tied to an exact
revision.

Do not use fragile line-number links as the sole identification of a function.
Name the symbol in prose.

## Diagram rule

Use Mermaid for relationships, ownership, workflows, sequences, and state
machines. Keep overview diagrams small; split complex systems into overview and
subsystem diagrams.

Every diagram needs prose that states what the arrows mean.

## Example rule

Use one consistent small Generic example throughout the User Guide. An example
must obey current validation and semantics. When code changes, update both
input values and expected trace/timeline.

## Status rule

Label future material explicitly:

- Implemented
- Limited
- Planned
- Candidate
- Deferred
- Research

Never write a proposed feature as a current instruction.

## Change checklist

Update documentation when:

- public interface changes;
- event/order/time semantics change;
- ownership/dependency direction changes;
- configuration/project schema changes;
- GUI workflow or action state changes;
- new limitation or failure behavior appears;
- test/conformance fixture changes;
- future plan is promoted or rejected.

## Review procedure

For a documentation PR:

```text
identify audited HEAD
-> render Markdown/Mermaid
-> check internal links
-> verify source/test paths
-> run examples mentally or with project
-> compare status against code
-> run appropriate tests
-> inspect diff for duplicated authority
```

## Avoid

- task-number diaries as primary guides;
- unexplained acronyms;
- long feature inventories without mental model;
- comments that restate code;
- unlinked path text where a clickable source is available;
- claims such as “fully tested” when manual/environment-specific checks were not
  executed;
- duplicating an ADR's rationale in several mutable guides.
