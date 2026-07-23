# Step 7 — Copy/Paste Prompts for the Remaining Work

Use one prompt at a time.

Before each prompt, ensure the previous work is safely checkpointed. Every prompt must begin by inspecting the latest `main` branch and reporting the exact HEAD SHA.

This prompt file was prepared against:

```text
Repository: gaochuanchao/CPSSim
HEAD: 99a61929a57f7c3ce446d9f355bb75b6b7f162b9
Commit message: update docs
```

A later HEAD is acceptable and expected.

## Prompt status

| Prompt | Related step | Status |
|---|---|---|
| A | Step 1 — Baseline audit | Complete |
| B | Step 2 — Shared structural edit controller | Complete |
| C | Step 3 — Architecture actions and context menus | Complete |
| D | Step 4 — Stable task ports | Complete |
| E | Step 5 — Interactive connections | Complete |
| F | Step 6 — Automated consolidation and regression | Next |
| G | Step 6 — Manual acceptance and handoff | After Prompt F |

---

# Prompt F — Architecture consolidation and automated regression

```text
Continue work on:

    https://github.com/gaochuanchao/CPSSim

Implement only the automated consolidation and documentation portion of:

    docs/refine/CPSSim_Architecture_Interaction_Plan/
    06_FINAL_CONSOLIDATION_AND_ACCEPTANCE.md

Before editing, inspect the latest main branch and report:

1. exact HEAD SHA and commit message;
2. git status;
3. registered Qt tests;
4. baseline configure/build/ctest result;
5. structural source of truth;
6. workspace-layout source of truth;
7. structural-selection source of truth;
8. structural Undo/Redo owner;
9. every persistent structural mutation path;
10. QtNodes and CPSSim shortcut ownership;
11. Generic/Bosch/read-only edit policy;
12. running-state restrictions;
13. save, close, Save As, open, and project-replacement behavior;
14. current regression coverage and confirmed gaps.

Do not edit until this audit is reported.

Current completed baseline:

- EditableSystemDraft is authoritative for editable structure.
- GuiArchitectureWorkspace is authoritative for task-node positions.
- StructuralSelection is authoritative for structural selection.
- QtStructuralEditController owns persistent structural mutations.
- One shared structural QUndoStack owns structural Undo/Redo.
- QtWorkbenchBridge coordinates refresh and selection synchronization.
- QtNodes is a graphics/interaction adapter, not persistent truth.
- Architecture actions and context menus are implemented.
- Every task has one input and one output port at index 0.
- Interactive Communication and Logical link creation/deletion is implemented.
- The loaded-link Delete issue is fixed.
- QtNodes conflicting structural shortcuts are suppressed.
- CPSSim remains the sole owner of structural Delete.

Current link invariants:

- directed;
- at most one link per ordered task pair;
- Communication and Logical kinds;
- both persisted in Generic projects;
- Communication solid;
- Logical dashed;
- Logical latency zero;
- Logical produces no network events;
- Communication latency may be zero or positive;
- fixed one-tick send offset for both;
- Source, Destination, and kind editable in System Builder;
- conversion preserves endpoints, selection, persistence, and Undo;
- Bosch adapter-owned dependencies protected.

Tasks:

1. Run baseline Qt configure, build, test listing, and full ctest.
2. Search globally across domain, draft, selection, controller,
   Architecture, System Builder, Explorer, persistence, project lifecycle,
   runtime, and tests.
3. Audit structural truth, layout truth, selection truth, shared Undo,
   bridge refresh, shortcut scope, policy, and run-state enforcement.
4. Audit Communication and Logical links across creation, rendering,
   editing, conversion, deletion, persistence, runtime, and Undo/Redo.
5. Add only missing, high-value tests.
6. Add or complete one chronological mixed structural Undo/Redo test.
7. Verify rejected operations add no undo command.
8. Verify project changes do not leak or resurrect stale undo history.
9. Verify save/reload for tasks, resources, assignments, execution profiles,
   node positions, and both link kinds.
10. Verify dirty-state and pending-editor Save behavior.
11. Fix only confirmed defects with minimal cross-layer changes.
12. Update AGENT-HANDOFF and the active Architecture-plan documents.
13. Keep component-library and palette drag/drop outside the active scope.
14. Run final configure, build, and full ctest.

Do not:

- implement a component library;
- implement palette drag/drop;
- redesign the workbench;
- redesign Explorer;
- create another controller or undo stack;
- mutate the draft directly from Architecture;
- make QtNodes persistent structural truth;
- add resource nodes;
- create endpoint-less links;
- change simulation, scheduling, FMU, event-ordering, or Bosch semantics;
- change the fixed send offset;
- add link kinds;
- make unrelated formatting changes;
- commit automatically.

Final report:

1. exact HEAD inspected;
2. baseline result;
3. confirmed issues;
4. files changed and rationale;
5. tests added/updated;
6. final test result;
7. ownership invariants verified;
8. link invariants verified;
9. documentation updated;
10. manual checks still required;
11. remaining risks;
12. concise diff summary.

Stop after automated consolidation and documentation.
```

---

# Prompt G — Final manual acceptance and handoff

```text
Continue work on:

    https://github.com/gaochuanchao/CPSSim

Perform only the manual-acceptance and handoff portion of:

    docs/refine/CPSSim_Architecture_Interaction_Plan/
    06_FINAL_CONSOLIDATION_AND_ACCEPTANCE.md

First inspect the latest main branch and report its exact HEAD SHA.

Do not add a new GUI feature.

Before updating the acceptance report:

1. run the full Qt configure/build/ctest sequence;
2. summarize automated coverage;
3. identify acceptance items already established by tests;
4. identify items requiring a human GUI session;
5. inspect AGENT-HANDOFF and the current Architecture-plan documents.

Tasks:

- prepare a final Architecture acceptance report;
- record exact HEAD and automated results;
- execute manual checks possible in the available environment;
- mark unavailable checks N/E rather than claiming success;
- record exact reproduction for every failure;
- record known limitations;
- confirm component-library and palette drag/drop remain outside scope;
- update AGENT-HANDOFF;
- state whether the Architecture interaction phase is COMPLETE or BLOCKED;
- recommend the next CPSSim development phase.

The critical manual matrix includes:

- Generic Task and Resource editing;
- Communication and Logical creation/edit/delete;
- loaded-link Delete;
- mixed Undo/Redo;
- selection synchronization;
- dirty state and Save/close/reopen;
- project replacement and Save As;
- running-state rejection;
- Bosch protection;
- dark/light themes;
- immediate canvas theme refresh;
- Fit, 100%, Auto Layout, and Snap;
- DPI transition when hardware is available;
- basic responsiveness and console-warning sanity.

Do not:

- introduce a component library or palette drag/drop;
- introduce new structural behavior;
- weaken project policy;
- alter simulation or FMU semantics;
- hide failing tests;
- claim unexecuted checks passed;
- mix an unplanned defect fix into the acceptance report;
- commit automatically.

If a reproducible code defect is found, stop and report:

1. exact reproduction;
2. expected and actual behavior;
3. project type and run state;
4. affected layer;
5. confirmed code facts;
6. likely cross-layer cause;
7. minimum files requiring change;
8. a focused correction prompt.

Final report:

1. exact HEAD;
2. configure/build/test result;
3. manual matrix result;
4. items not executed;
5. documentation updated;
6. known limitations;
7. residual defects;
8. COMPLETE or BLOCKED decision;
9. recommended next development goal.
```

---

# Focused residual-defect correction prompt

Use only when Prompt F or G identifies a reproducible defect.

```text
Stop unrelated work.

Inspect the latest main branch of gaochuanchao/CPSSim and report the exact
HEAD before editing.

Correct only this reproducible defect:

    [INSERT EXACT REPRODUCTION]

Before editing, trace the issue globally through:

- domain model;
- EditableSystemDraft;
- run assignments;
- StructuralSelection;
- QtStructuralEditController;
- QtWorkbenchBridge;
- Architecture model/view;
- System Builder;
- Experiment Explorer;
- persistence;
- project lifecycle;
- runtime;
- Undo/Redo;
- tests.

Distinguish confirmed code facts from hypotheses.

Preserve:

- EditableSystemDraft as structural truth;
- GuiArchitectureWorkspace as position truth;
- StructuralSelection as selection truth;
- one QtStructuralEditController;
- one structural QUndoStack;
- bridge-driven refresh;
- Generic/Bosch policy;
- run-state protection;
- Communication/Logical semantics;
- fixed one-tick send offset;
- CPSSim ownership of structural Delete.

Do not apply a view-only workaround.
Do not add an unrelated feature.
Do not add a component palette or palette drag/drop.
Do not commit automatically.

Add a regression test that fails before the fix and passes after it.
Run the full Qt configure, build, and ctest.
Report root cause, files changed, tests, and remaining risks.
```
