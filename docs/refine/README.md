# CPSSim GUI Refinement Plan

This folder defines the next GUI refinement work. The plan is divided into four goals:

1. `G1_PROJECT_STARTUP_AND_FILES.md`
2. `G2_SYSTEM_BUILDER.md`
3. `G3_WORKBENCH_USABILITY.md`
4. `G4_RESULTS_AND_EXPORT.md`

Use `CODEX_EXECUTION_RULES.md` for every coding session and `TASK_TEMPLATE.md` when creating a smaller implementation task.

## Recommended order

```text
G1 Project startup and files
        ↓
G2 System builder
        ↓
G3 Workbench usability
        ↓
G4 Results and export
```

Small isolated G3 changes may be implemented while G1 is in progress.

## Important scope boundary

These files cover GUI usability and experiment workflow only. They do not authorize:

- new scheduling semantics;
- multi-vehicle Bosch support;
- context-aware scheduling policies;
- richer network models;
- direct Simulink-style canvas editing;
- batch parameter sweeps.

Create separate design tasks for those capabilities after this refinement plan is complete.
