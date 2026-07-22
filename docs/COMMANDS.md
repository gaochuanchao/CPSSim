# CPSSim Command Handbook

Run commands from the repository root. CMake owns build and test logic; the
root Makefile intentionally exposes only the six normal user commands below.

## Public commands

| Command | Behavior |
|---|---|
| `make` | Configure the normal development application build and compile the core, CLI, GUI, Bosch adapters, and supplied Linux FMU runtime without launching or testing |
| `make run-cli` | Build the CLI and required Bosch runtime, then open the persistent terminal interface |
| `make run-gui` | Build and launch the Dear ImGui workbench |
| `make test` | Open the verification menu on a terminal; run quick verification when input is non-interactive |
| `make clean` | Remove only the documented generated build directories below |
| `make help` | List only this public interface |

Quick start:

```bash
make
make run-cli
```

The normal Make application configuration is `make-dev`, stored in
`build/make-dev/`. It enables the GUI because the default build promises both
user interfaces. CMake still builds only the explicit application targets for
`make`, so launching an interface does not compile the complete test suite.

## Terminal interface

`make run-cli` opens this shell:

```text
CPSSim 0.1.0
Interactive terminal interface
Type "help" to list commands.

cpssim>
```

Initial commands are:

| CLI command | Behavior |
|---|---|
| `help [command]` | List registered commands or show one command's usage |
| `list` | List available experiment families and predefined choices |
| `run bosch` | Open the Bosch Challenge experiment wizard |
| `version` | Print the compiled CPSSim version |
| `quit` / `exit` | Leave the shell cleanly |

The Bosch wizard selects one supplied trajectory (`example_v_10`,
`example_v_12_5`, or `example_v_15`), one validated timing scenario
(`dedicated` or `shared_cloud`), and either the complete input or an inclusive
nonnegative stop tick. It shows the request before executing through the
normal scheduling, networking, trigger, and FMI path.

Scripts should bypass prompts and call the same executable directly:

```bash
./build/make-dev/cpssim_cli run bosch \
  --example examples/example_v_10 \
  --scenario shared_cloud \
  --stop-tick 150000
```

Omit `--stop-tick` to use the complete supplied trajectory. Direct Bosch runs
require `--example` and `--scenario`, reject trajectories outside the three
supplied names, and return a nonzero status for parsing, loading, FMI, or
execution failures.

The compatibility executable `cpssim_bosch_example` remains an internal CMake
integration target. It is not a public Make command.

## Verification interface

`make test` presents this menu when stdin is a terminal:

```text
CPSSim verification

1. Quick verification
2. Test a specific module
3. Run all tests
4. Run full verification
5. Formatting
6. List test modules
7. Exit
```

The modes mean:

- **Quick verification** checks formatting without modifying files, builds the
  Debug preset, and runs all normal tests.
- **Test a specific module** builds Debug and selects tests by their primary
  CTest label.
- **Run all tests** builds Debug and runs every registered normal test with
  failure output.
- **Run full verification** runs formatting, Debug, ASan/UBSan, and Release;
  Clang and clang-tidy also run when their tools are installed.
- **Formatting** offers an explicit non-mutating check or source-modifying
  apply operation. Tests never format source as a side effect.

CI, scripts, and coding agents use the direct verification driver:

```bash
./scripts/verify.sh quick
./scripts/verify.sh all
./scripts/verify.sh full
./scripts/verify.sh module fmi
./scripts/verify.sh module bosch
./scripts/verify.sh format-check
./scripts/verify.sh format-apply
./scripts/verify.sh list-modules
```

With no argument and non-interactive stdin, the driver selects `quick` rather
than waiting for a prompt. Unknown modes and labels fail with status 2. A
missing required tool also fails explicitly; optional Clang/clang-tidy stages
in `full` report when they are skipped.

The tidy preset analyzes production libraries and applications. Catch2 test
targets are excluded from clang-tidy because its assertion decomposition
triggers false chained-comparison diagnostics; those targets still compile
with project warnings as errors and all tests execute under the tidy preset.

## CTest labels and focused tests

The maintained primary module labels are:

```text
core config kernel scheduler network functional fmi bosch conformance gui cli
```

CTest labels classify behavior; the verification driver does not know test
executable names. To inspect registered tests or select labels directly:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev -N
ctest --test-dir build/dev --output-on-failure -L '^scheduler$'
```

New tests belong in the closest `tests/<module>/` directory. Add their source
to the matching CMake test executable and assign or reuse a maintained CTest
label. Do not add a Make target. Add a new primary label to
`scripts/verify.sh` only when the repository gains a genuinely new test
module.

## Adding a CLI command

CLI commands live under [`apps/cli/commands/`](../apps/cli/commands/). Each
implements `CliCommand` metadata (`name`, `description`, and `usage`) plus one
execution method. A private command factory is declared in
`command_factories.hpp`; [`command_registry.cpp`](../apps/cli/command_registry.cpp)
is the only registration point.

Direct argv and interactive lines both dispatch through `CliApplication` and
the same registry. Interactive lines are tokenized by `command_parser.cpp`;
commands receive already separated arguments. The Bosch command turns either
direct options or wizard choices into `BoschRunRequest`, then invokes the same
injected `BoschRunService`. Simulation behavior belongs in application
services or simulator modules, never in terminal prompts.

Parser, shell, and prompt tests belong under `tests/cli/` and inject streams
and mock services; they must not launch a real terminal. Add the command and
direct syntax to this handbook and update `help` metadata through the command
implementation. No Makefile edit is needed.

## GUI build and launch

During the Goal 7 parity period the existing `gui` preset builds Dear ImGui,
while `qt-gui` builds the native Qt 6 Widgets shell and `gui-both` verifies
both frontends:

```bash
cmake --preset qt-gui
cmake --build --preset qt-gui --target cpssim_qt_gui
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qt-gui -R cpssim_qt_gui_tests
```

Qt 6.4 or newer Core, Gui, Widgets, Test, and OpenGLWidgets development
components are required. QtNodes is fetched only for the Qt frontend at the
immutable commit recorded in ADR-0026.

Launch the tracked example with its default inclusive horizon:

```bash
make run-gui
```

The executable retains its direct arguments:

```bash
./build/make-dev/cpssim_gui config/examples/basic.json 500
./build/make-dev/cpssim_gui \
  config/examples/basic.json 500 --mock-functional
```

On Ubuntu, install the GLFW/OpenGL development boundary first:

```bash
sudo apt install libglfw3-dev libgl1-mesa-dev
```

The first application configuration may download the SHA-256-pinned Dear
ImGui source. DPI scaling, monitor changes, and GUI text scaling remain
presentation-only and cannot alter simulation behavior. See the
[GUI tutorial](gui/README.md).

## Direct development presets

Advanced verification remains available through the non-interactive driver or
direct CMake presets instead of additional Make commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Use `clang` or `tidy` in place of the preset name for the supported second
compiler and static-analysis configurations. CMake remains authoritative.

The JSON adapter first looks for `nlohmann-json3-dev`; otherwise CMake
downloads the pinned source into `build/_deps/`. GUI configuration behaves
similarly for Dear ImGui.

## Clean behavior

`make clean` removes these generated directories when present:

```text
build/make-dev
build/dev
build/release
build/asan
build/clang
build/tidy
build/gui
build/make-release
build/make-asan
build/make-gui
build/_deps
```

It does not remove sources, documentation, configurations, examples,
reference artifacts, run plans, or saved results. Normally do not clean;
incremental CMake builds are faster.

## Reference artifact integrity

After intentionally moving or changing a pinned MATLAB/Simulink input or
output, verify its recorded digest:

```bash
sha256sum -c experiments/bosch_v10_reference/checksums.sha256
```

Golden artifacts must not be regenerated merely to hide an unexplained
conformance mismatch.
