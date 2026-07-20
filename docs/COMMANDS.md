# CPSSim Command Handbook

This is the short terminal reference for day-to-day development. Run these
commands from the repository root.

For a guided workflow and test-selection examples, see the
[developer guide](guide/DEVELOPER-GUIDE.md). CMake owns the build graph, the
root Makefile provides short commands, Catch2 defines test cases, CTest runs
them, and sanitizers check runtime memory and undefined behavior.

## Quick decision table

| Purpose | Command |
|---|---|
| See the available shortcuts | `make help` |
| Compile the normal Debug build | `make` or `make debug` |
| Compile and run tests | `make test` |
| Compare both captured scheduler/network/trigger scenarios | `make conformance` |
| Run only the Bosch FMI lifecycle tests | `make fmi-test` |
| Run online functional and replay conformance | `make functional-test` |
| Run one supplied Bosch example | `make bosch-example` |
| Run all three full Bosch examples | `make bosch-examples` |
| Run the current command-line application | `make run` |
| Build the optional Dear ImGui application | `make gui` |
| Build and launch the Dear ImGui application | `make run-gui` |
| Apply C++ formatting | `make format` |
| Check formatting without changing files | `make format-check` |
| Validate the tracked JSON example's syntax | `jq empty config/examples/basic.json` |
| Test for memory and undefined-behavior errors | `make asan` |
| Build and test optimized code | `make release` |
| Remove Make-generated build trees | `make clean` |
| Check a patch for whitespace errors | `git diff --check` |
| Inspect changed and untracked files | `git status --short` |

## After adding or changing C++ code

Use this minimum loop while developing:

```bash
make format
make test
```

Run the CLI when the change affects executable behavior:

```bash
make run
```

Before considering the task complete, use the deeper checks:

```bash
make format-check
make asan
make release
git diff --check
git status --short
```

`make test` is the most important everyday command. It configures the Debug
build, compiles changed files, and runs the test suite. Compilation warnings
are errors, so it also catches the enabled compiler diagnostics.

The configuration test suite loads the tracked JSON example, so `make test` checks its
schema and model invariants in addition to its JSON syntax.

The Bosch scenario commands are registered with CTest. Therefore `make test`
also runs dedicated and shared-cloud scheduler, network, and trigger
conformance. Use the following when you want only the readable report:

```bash
make conformance
```

See the [Bosch conformance module](modules/bosch-timing-conformance.md) for
scheduler/network normalization and the
[trigger-adapter module](modules/bosch-trigger-adapter.md) for the sixteen
trigger mappings and exporter.

The FMI tests compile the supplied Bosch C source into a build-tree Linux
shared library, then load and execute it through the generic adapter:

```bash
make fmi-test
```

See the [FMI module](modules/fmi2-co-simulation.md) for lifecycle ownership,
the generated test library, and the reason no extra FMI package is currently
required.

Online functional interaction connects that importer to the normal event engine. Run its mock, short
Bosch, dedicated-reference, shared-cloud-reference, and exact replay checks:

```bash
make functional-test
```

See the
[online-functional module](modules/online-functional-interaction.md) and
[ordering ADR](adr/0017-order-online-functional-observation-before-same-tick-actions.md)
for call ordering and typed observations. Numerical tolerances are recorded in
the [Bosch reference README](../experiments/bosch_v10_reference/README.md).

Run the complete `example_v_10` input with the validated shared-cloud plan:

```bash
make bosch-example
```

Choose another input, run plan, or shorter inclusive horizon with Make
variables:

```bash
make bosch-example \
  BOSCH_EXAMPLE_DIR=examples/example_v_15 \
  BOSCH_SCENARIO=dedicated \
  BOSCH_STOP_TICK=150000
```

Run all three supplied 1,500,000-row datasets through their full available
horizon with:

```bash
make bosch-examples
```

These commands use the build-tree Linux shared library compiled from the
supplied Bosch FMU source. The runner validates all six example CSV columns;
the v10 FMU consumes the two feedforward sequences and velocity. See
[the Bosch example instructions](../examples/readme_examples.md) for the exact
capability boundary.

## Build and run the optional GUI

The normal build remains headless. Build the optional target explicitly:

```bash
make gui
```

Launch the tracked example through the default 300-tick horizon:

```bash
make run-gui
```

Or pass an experiment file and inclusive stop tick directly:

```bash
./build/make-gui/cpssim_gui config/examples/basic.json 500
```

Attach the deterministic mock functional source when learning or customizing
the signal view without an FMU:

```bash
./build/make-gui/cpssim_gui \
  config/examples/basic.json 500 --mock-functional
```

At startup GLFW scales the native content area for the monitor where the window
is placed. CPSSim then follows that window's monitor scale. Moving the GUI
between displays automatically resizes the native content area where required
and rescales text, spacing, and panel layout. Use `View` -> `Text size` to apply
an additional personal text multiplier without changing simulation behavior.
Display scale and text size are presentation-only; the text multiplier is not
persisted.

On a new Ubuntu machine, install the GLFW/OpenGL development boundary first:

```bash
sudo apt install libglfw3-dev libgl1-mesa-dev
```

CMake downloads the SHA-256-pinned Dear ImGui v1.92.8 source only for a GUI
configuration. The first `make gui` therefore needs network access. Headless
`make test`, adapters, and conformance commands neither download nor link GUI
code. See the [module interactions](MODULE-INTERACTIONS.md#gui-boundary) and
[GUI ADR](adr/0018-use-a-single-threaded-snapshot-command-gui-boundary.md) for
the ownership and determinism rules. The
[GUI tutorial](gui/README.md) walks through the first run, every panel, and
common customization changes.

## After changing documentation only

```bash
git diff --check
make test
```

The first command checks patch formatting. The second confirms that moved or
edited documentation did not accidentally disturb the build workflow.

## Run a focused test

Build the normal Debug tree once, list its registered test names, then select
tests with a CTest regular expression:

```bash
make debug
ctest --test-dir build/make-dev -N
ctest --test-dir build/make-dev --output-on-failure -R 'event queue|same-tick'
```

Replace the final expression with a concept visible in the test list, such as
`scheduler`, `message`, `functional`, `GUI`, or `fmi2`. Run `make test` before
handoff even when the focused cases pass.

## Debug, Release, and sanitizer builds

### Debug: normal development

```bash
make
make debug
```

These commands use the same `Debug` configuration. `make` is shorter;
`make debug` makes the selected mode explicit. Debug builds retain debug
information and assertions and are the normal choice while implementing and
testing behavior.

### Release: optimized validation

```bash
make release
```

This creates `build/make-release/`, compiles with CMake's `Release`
configuration, and runs the tests. Release builds enable compiler
optimizations and normally define `NDEBUG`, which disables standard `assert`
checks. Use Release for performance measurements and to catch problems that
appear only under optimization. Do not use it instead of Debug tests.

### ASan and UBSan: runtime defect detection

```bash
make asan
```

This builds and tests with AddressSanitizer and UndefinedBehaviorSanitizer.
It is slower than the normal Debug build, so it is a completion check rather
than the command required after every small edit.

## Static analysis and a second compiler

Run clang-tidy when C++ code changes substantially or before review:

```bash
cmake --preset tidy
cmake --build --preset tidy
ctest --preset tidy --output-on-failure
```

Check that the project also builds with Clang:

```bash
cmake --preset clang
cmake --build --preset clang
ctest --preset clang --output-on-failure
```

These use direct CMake commands because the root Makefile only provides the
most common shortcuts. CMake remains the authoritative build configuration.

## JSON dependency on a fresh machine

The JSON adapter uses nlohmann/json without exposing it in model interfaces.
CMake first looks for the
Ubuntu package:

```bash
sudo apt install nlohmann-json3-dev
```

Installing it is optional. If it is absent, the first CMake configuration
downloads the SHA-256-pinned v3.11.3 source into `build/_deps/`. That first
configuration therefore needs network access; later builds reuse the local
cache. See the
[experiment-configuration module](modules/experiment-configuration.md) for the
dependency boundary and schema contract.

## Validate the MATLAB reference artifacts

Run this after moving or intentionally changing a pinned reference input or
output:

```bash
sha256sum -c experiments/bosch_v10_reference/checksums.sha256
```

Every entry must report `OK`. Do not regenerate golden artifacts merely to
hide an unexplained mismatch.

## Direct CMake equivalents

The Make shortcuts delegate to these CMake presets:

| Make command | CMake configuration | Build directory |
|---|---|---|
| `make` or `make debug` | `make-dev` (`Debug`) | `build/make-dev/` |
| `make release` | `make-release` (`Release`) | `build/make-release/` |
| `make asan` | `make-asan` (`Debug` plus sanitizers) | `build/make-asan/` |

For example, the direct Release workflow is:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

## When to clean

Normally, do not clean: CMake and the compiler rebuild only what changed.
Use this after changing compilers, encountering stale generated files, or when
you deliberately want a fresh Make-generated build:

```bash
make clean
make test
```

`make clean` removes only `build/make-dev/`, `build/make-release/`,
`build/make-asan/`, and `build/make-gui/`. It does not delete source files,
documentation, experiment artifacts, or the direct Ninja build trees.

## Practical completion checklist

For a normal C++ task:

```bash
make format
make test
make asan
make release
git diff --check
git status --short
```

Also run task-specific integration or conformance commands when the current
task changes scheduling, traces, networking, configuration, or FMU behavior.
Report the exact commands and results in the change handoff or review, and
update current validation documentation when the supported baseline changes.
