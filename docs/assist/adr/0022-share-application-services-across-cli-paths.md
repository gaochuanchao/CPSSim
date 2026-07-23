# ADR-0022: Share Application Services Across CLI Paths

- Status: Accepted
- Date: 2026-07-20
- Owners: CPSSim contributors
- Related work: Make interface refinement, F1

## Context

CPSSim needs a persistent terminal shell, reproducible direct commands, and a
Bosch wizard. The validated Bosch execution path previously lived in
`apps/bosch_example/main.cpp`, while `cpssim_cli` printed only the version.
Copying that orchestration into interactive prompts or direct parsing would
create multiple execution implementations and make equivalence difficult to
test.

The generic core must remain independent of Bosch, FMI, and terminal I/O.
Terminal tests also need to exercise prompts and failures without loading
1,500,000-row trajectories or a real FMU.

## Decision

Introduce an application layer above adapters:

- `BoschRunRequest` contains parser-independent trajectory, FMU, scenario,
  horizon, reference, and instance inputs;
- injectable `BoschRunService` is the stable command-to-execution boundary;
- `DefaultBoschRunService` alone loads the supplied trajectory, constructs the
  Bosch FMI model, invokes `run_bosch_reference_online`, and returns detached
  summary values;
- the CLI wizard and direct options both construct the same request and call
  the same service;
- the legacy Bosch executable temporarily calls the service as an internal
  compatibility target; and
- a registry of small `CliCommand` objects owns terminal discovery and
  dispatch. `main.cpp` only constructs dependencies and forwards argv.

The application service may depend on Bosch, FMI, conformance, and core
libraries. `cpssim_core` must not depend on the application service or CLI.
Terminal streams are injected into `CliApplication`, and Bosch execution is
injected through `BoschRunService`, so parser and wizard tests use a mock.

## Consequences

Positive:

- equivalent direct and interactive choices share one execution path;
- a command is added at one registry point without editing the Makefile;
- parsing and prompt tests are deterministic and do not require a terminal;
- the legacy executable preserves compatibility without owning simulation
  logic; and
- adapter and core dependency directions remain unchanged.

Limiting:

- the first service supports only the three supplied single-vehicle Bosch
  trajectories and two validated reference scenarios;
- CLI path defaults assume execution from the repository root and locate the
  supplied Linux model beside `cpssim_cli`; and
- general JSON experiment execution, trace files, and manifests remain the
  unimplemented part of F1.

## Validation

CLI tests cover tokenizer errors, shell EOF, unknown commands, help, version,
both exit spellings, direct Bosch validation, wizard request construction, and
service failures. Existing short real-FMU example tests now call
`DefaultBoschRunService`; timing and functional conformance remain unchanged.
