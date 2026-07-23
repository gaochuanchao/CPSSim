# Module: Application Interfaces

## Responsibility

Provide user-facing CLI command organization and reusable experiment
orchestration above the simulator and adapter libraries. The module converts
terminal choices into typed requests; it does not define scheduling,
networking, trigger, FMI, or event-ordering behavior.

## Public interfaces

- [`bosch_run_service.hpp`](../../src/cpssim/application/bosch_run_service.hpp)
  defines `BoschRunRequest`, `BoschRunSummary`, injectable `BoschRunService`,
  and the production `DefaultBoschRunService`.
- [`cli_application.hpp`](../../apps/cli/cli_application.hpp) coordinates
  direct argv execution and the persistent interactive shell over injected
  streams and a run service.
- [`command_registry.hpp`](../../apps/cli/command_registry.hpp) defines command
  metadata, context, results, and the single registration owner.
- [`command_parser.hpp`](../../apps/cli/command_parser.hpp) tokenizes only
  interactive terminal lines. Direct argv bypasses tokenization.
- [`system_builder_workflow.hpp`](../../src/cpssim/application/project/system_builder_workflow.hpp)
  validates an `EditableSystemDraft` plus pending stop tick, policy, and
  explicit assignments, then builds one complete replacement `ProjectContext`
  before application-state mutation.

## Ownership and flow

```text
direct argv ---------+
                     +-> CommandRegistry -> BoschRunRequest
interactive parser --+                         |
                                               v
                                    BoschRunService
                                               |
                         existing Bosch / FMI / core execution
```

`CommandRegistry` owns command objects. `CliApplication` owns the registry and
immutable path context but not streams or the run service. A Bosch command
owns only temporary parsing/prompt state. `DefaultBoschRunService` is
stateless; the existing `BoschFmi2FunctionalModel`, functional runtime, and
simulation engine own mutable execution state for each call.

## Invariants

- the registry constructor is the only command-registration point;
- empty argv enters the shell and nonempty argv executes exactly one command;
- injected terminal streams use the named `CliStreams` bundle so output and
  error channels cannot be exchanged positionally;
- direct and wizard Bosch paths create the same `BoschRunRequest` and call the
  same service interface;
- supported trajectories are the three supplied `example_v_*` datasets;
- stop ticks are nonnegative integers and cannot exceed the loaded trajectory;
- terminal rendering never changes canonical events or functional values; and
- no application or CLI type is a dependency of `cpssim_core`.

## Dependencies

`cpssim_cli_support` depends on `cpssim_core` for version reporting and on
`cpssim_bosch_application` for the injectable service contract.
`cpssim_bosch_application` depends inward on the Bosch FMI adapter and Bosch
reference runner. The internal compatibility executable and public CLI both
depend on this application service; neither duplicates the run sequence.
Generic project reconstruction remains in `cpssim_gui_support`; it depends on
public configuration, run-plan, project, and functional-factory boundaries but
not on Dear ImGui.

## Failure behavior

Malformed terminal syntax, unknown commands/options, invalid trajectories,
invalid scenarios, and invalid stop ticks return status 2 in direct mode.
Loading, FMI, and simulator exceptions cross the service boundary and are
rendered as one Bosch run error. In an interactive shell a failed command is
reported and the shell remains available; EOF closes it cleanly.

## Verification

- [`cli_application_test.cpp`](../../tests/cli/cli_application_test.cpp)
- [`example_data_test.cpp`](../../tests/bosch/example_data_test.cpp)
- `./scripts/verify.sh module cli`
- `./scripts/verify.sh module functional`
- [ADR-0022](../adr/0022-share-application-services-across-cli-paths.md)
