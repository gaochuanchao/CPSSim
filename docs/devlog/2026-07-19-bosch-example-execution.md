# 2026-07-19 — Bosch Example Execution

## Scope

Added a Bosch-specific executable workflow for the three supplied trajectory
directories without changing generic simulator time, scheduling, networking,
or functional-model semantics.

## Implemented boundary

- `load_bosch_example_trajectory()` opens the six required CSV columns in row
  lockstep, validates equal lengths and finite values, requires positive
  velocity, and converts the decimal 0.1 ms time vector to consecutive integer
  ticks exactly.
- `cpssim_bosch_example` runs the parsed feedforward and velocity samples
  through the normal `SimulationEngine`, Bosch trigger projection, and real
  FMI model.
- Runs select one already validated single-vehicle `dedicated` or
  `shared_cloud` reference plan because the Bosch example data does not define
  deadlines, offsets, priorities, or one mandatory allocation.
- `make bosch-example` runs one selected input and `make bosch-examples` runs
  all three complete supplied inputs.

## Validation evidence

Focused Debug tests loaded all 1,500,000 rows from `example_v_10`,
`example_v_12_5`, and `example_v_15`, then executed every dataset through a
short real-FMU/normal-engine smoke horizon. All three focused tests passed.

Complete Release runs used the shared-cloud reference plan through inclusive
stop tick `1499999`. Every run produced 1,500,000 functional observations and
292,499 canonical events:

| Input | Result | Elapsed | Peak RSS |
|---|---|---:|---:|
| `example_v_10` | completed | 43.19 s | 1,438,820 KiB |
| `example_v_12_5` | completed | 45.88 s | 1,439,048 KiB |
| `example_v_15` | completed | 45.36 s | 1,438,824 KiB |

Completion validation:

- `make test`: 116/116 Debug tests passed in 27.93 seconds;
- `make release`: 116/116 Release tests passed in 5.29 seconds;
- `make asan`: 116/116 ASan/UBSan tests passed in 123.01 seconds;
- `make format-check`: passed;
- `git diff --check`: passed; and
- `sha256sum -c experiments/bosch_v10_reference/checksums.sha256`: every
  pinned source and reference artifact passed.

## Limitations

- One full run retains the append-only typed functional trace and currently
  peaks near 1.4 GiB; `make bosch-examples` launches separate processes so the
  three traces are not resident together.
- Position x/y columns are validated but are reference/visualization data, not
  Bosch v10 FMU inputs.
- This proves single-vehicle execution of the supplied formats. It does not
  add multiple functional-model instances, probabilistic PERT execution time,
  three-core scheduling domains, or a constraint-ranking report.
- The final supplied time row is 149.9999 seconds, so the default inclusive
  horizon is tick `1499999`; no unsupplied 150.0000-second input is invented.
