# Adapter Symbol Index

## FMI 2.0 importer

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `Fmi2ModelInfo` | [`fmi2_importer.hpp`](../../../src/cpssim/fmi/fmi2_importer.hpp) | aggregate | prepared library metadata |
| `Fmi2CoSimulation::Fmi2CoSimulation` | `fmi2_importer.hpp` | [`fmi2_importer.cpp`](../../../src/cpssim/fmi/fmi2_importer.cpp) | load library/function table |
| `initialize` overloads | `fmi2_importer.hpp` | `fmi2_importer.cpp` | instantiate/init/parameters |
| `set_real/get_real` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | typed Real access |
| `set_integer/get_integer` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | typed Integer access |
| `set_boolean/get_boolean` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | typed Boolean access |
| `set_string/get_string` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | typed String access |
| `do_step` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | synchronous communication step |
| `terminate` | `fmi2_importer.hpp` | `fmi2_importer.cpp` | terminate/free component |

## Bosch functional adapter

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `BoschTrajectorySample` | [`bosch_fmi2_functional_model.hpp`](../../../src/cpssim/bosch/bosch_fmi2_functional_model.hpp) | aggregate | environment input |
| `BoschFmi2FunctionalModel` constructor | same | [`bosch_fmi2_functional_model.cpp`](../../../src/cpssim/bosch/bosch_fmi2_functional_model.cpp) | load FMU/validate trajectory |
| `initialize` | same | `.cpp` | Bosch parameters and tick-zero output |
| `advance_to` | same | `.cpp` | trajectory/triggers/doStep/output |
| `apply_actions` | same | `.cpp` | canonical events -> Bosch pulses |
| `observation` | private | `.cpp` | read six typed outputs |
| `finalize` | same | `.cpp` | terminate once |

## Bosch trigger and data

| Component | Declaration | Implementation | Role |
|---|---|---|---|
| trigger encoder | [`trigger_encoder.hpp`](../../../src/cpssim/bosch/trigger_encoder.hpp) | [`trigger_encoder.cpp`](../../../src/cpssim/bosch/trigger_encoder.cpp) | event -> explicit trigger mapping |
| example/trajectory data | [`example_data.hpp`](../../../src/cpssim/bosch/example_data.hpp) | [`example_data.cpp`](../../../src/cpssim/bosch/example_data.cpp) | strict supplied input loading |
| project factory | [`bosch_project_factory.hpp`](../../../src/cpssim/application/bosch_project_factory.hpp) | [`bosch_project_factory.cpp`](../../../src/cpssim/application/bosch_project_factory.cpp) | Bosch project creation |
| workbench services | [`bosch_workbench_services.hpp`](../../../src/cpssim/application/bosch_workbench_services.hpp) | [`bosch_workbench_services.cpp`](../../../src/cpssim/application/bosch_workbench_services.cpp) | runtime resolver/signal metadata |
| result analysis | [`bosch_result_analysis.hpp`](../../../src/cpssim/application/bosch_result_analysis.hpp) | [`bosch_result_analysis.cpp`](../../../src/cpssim/application/bosch_result_analysis.cpp) | scenario-specific completed analysis |
| run service | [`bosch_run_service.hpp`](../../../src/cpssim/application/bosch_run_service.hpp) | [`bosch_run_service.cpp`](../../../src/cpssim/application/bosch_run_service.cpp) | shared CLI/example orchestration |

## Conformance

| Symbol/component | Declaration | Implementation | Role |
|---|---|---|---|
| timing/trigger reference | [`bosch_reference.hpp`](../../../src/cpssim/conformance/bosch_reference.hpp) | [`bosch_reference.cpp`](../../../src/cpssim/conformance/bosch_reference.cpp) | exact captured comparison |
| functional reference | [`bosch_functional_reference.hpp`](../../../src/cpssim/conformance/bosch_functional_reference.hpp) | [`bosch_functional_reference.cpp`](../../../src/cpssim/conformance/bosch_functional_reference.cpp) | numerical trajectory comparison |
| conformance executable | — | [`apps/conformance/main.cpp`](../../../apps/conformance/main.cpp) | terminal runner |
| captured reference | — | [`experiments/bosch_v10_reference/`](../../../experiments/bosch_v10_reference/) | provenance/checksums/data |

## CLI adapters

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `CliCommand` | [`command_registry.hpp`](../../../apps/cli/command_registry.hpp) | command class | command interface |
| `CommandRegistry` | `command_registry.hpp` | [`command_registry.cpp`](../../../apps/cli/command_registry.cpp) | ownership/discovery |
| `CliApplication` | [`cli_application.hpp`](../../../apps/cli/cli_application.hpp) | [`cli_application.cpp`](../../../apps/cli/cli_application.cpp) | shell/direct dispatch |
| command parser | [`command_parser.hpp`](../../../apps/cli/command_parser.hpp) | [`command_parser.cpp`](../../../apps/cli/command_parser.cpp) | input tokenization |
| Bosch run command | command implementation | [`commands/bosch_run_command.cpp`](../../../apps/cli/commands/bosch_run_command.cpp) | request -> run service |
