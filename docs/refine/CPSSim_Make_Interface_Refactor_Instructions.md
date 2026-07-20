# CPSSim Make Interface Refactor — Code-Agent Instructions

## 1. Objective

Refactor the repository-level command interface so that users see only a small, coherent set of Make commands:

```bash
make
make run-cli
make run-gui
make test
make clean
make help
```

The naming must be explicit and symmetric:

- `run-cli` launches the terminal-based CPSSim interface.
- `run-gui` launches the graphical CPSSim workbench.
- `make` builds the normal CPSSim applications and required runtime components.
- `test` provides one entry point for code and logic verification.
- `clean` removes generated build artifacts safely.
- `help` documents only the public command interface.

The current repository contains many Make targets because build modes, test subsets, Bosch examples, sanitizers, formatting, and GUI operations were added incrementally. These capabilities should remain available internally, but they must no longer appear as unrelated top-level user commands.

---

## 2. Required public behavior

### 2.1 `make`

`make` is the default build command.

It must:

1. configure the normal development build when necessary;
2. compile the simulator core;
3. compile `cpssim_cli`;
4. compile `cpssim_gui`;
5. compile Bosch Challenge runtime support required by the CLI or GUI;
6. avoid automatically launching either interface;
7. avoid automatically running the complete verification workflow.

The implementation may continue using CMake presets internally. Do not move build logic into the Makefile.

Expected use:

```bash
make
```

Expected outcome:

```text
CPSSim and its normal user applications are built successfully.
```

Do not retain public aliases such as `make build`, `make debug`, `make configure`, or `make gui`.

---

### 2.2 `make run-cli`

`make run-cli` must build the required CLI target if necessary and then launch the terminal-based CPSSim interface.

Expected use:

```bash
make run-cli
```

When launched without command-line arguments, `cpssim_cli` must enter an interactive command shell rather than only printing the version.

Initial shell behavior should be intentionally small and extensible:

```text
CPSSim <version>
Interactive terminal interface
Type "help" to list commands.

cpssim>
```

Minimum initial commands:

```text
help
list
run bosch
version
quit
exit
```

Suggested semantics:

- `help`: show available commands and concise usage.
- `list`: show available experiment families or predefined scenarios.
- `run bosch`: start an interactive Bosch Challenge experiment wizard.
- `version`: print the CPSSim version.
- `quit` / `exit`: leave the shell cleanly.

The Bosch wizard should initially expose the capabilities already present in the repository:

1. select one supplied trajectory:
   - `example_v_10`;
   - `example_v_12_5`;
   - `example_v_15`;
2. select one scenario:
   - `dedicated`;
   - `shared_cloud`;
3. select:
   - complete supplied trajectory; or
   - a user-provided nonnegative stop tick;
4. display a summary before execution;
5. run through the existing Bosch scheduling, networking, trigger, and FMI path;
6. print the existing run summary and final observations.

Do not duplicate Bosch simulation logic inside the CLI. Extract or reuse an application-level Bosch run service currently used by `apps/bosch_example/main.cpp`.

#### Non-interactive CLI support

Interactive use must not be the only interface. The same executable should support direct commands for scripts and reproducible experiments, for example:

```bash
./build/<normal-build-dir>/cpssim_cli run bosch \
  --example examples/example_v_10 \
  --scenario shared_cloud \
  --stop-tick 150000
```

The interactive wizard and direct command must invoke the same parser-independent execution service.

`make run-cli` does not need to forward arbitrary arguments initially. Users and automation may call `cpssim_cli` directly for non-interactive commands. An optional later convenience is:

```bash
make run-cli ARGS="run bosch --example examples/example_v_10 --scenario shared_cloud"
```

Do not make this optional convenience a prerequisite for the refactor.

---

### 2.3 CLI extensibility requirement

Do not implement the interactive shell as one large `if/else` block in `main.cpp`.

Introduce an application-layer command organization with a clear registration point. A suitable conceptual structure is:

```text
apps/cli/
├── main.cpp
├── cli_application.*
├── command_registry.*
├── command_parser.*
└── commands/
    ├── help_command.*
    ├── list_command.*
    ├── version_command.*
    └── bosch_run_command.*
```

Exact filenames may differ if a better repository-consistent organization exists.

Each command should define or expose:

- command name;
- short description;
- accepted arguments/options;
- direct execution path;
- optional interactive prompting path.

The command registry should be the only place where a new CLI command is added.

Adding a new CLI command must not require editing the root Makefile.

Add a concise developer guide explaining:

1. where CLI commands live;
2. how a command is registered;
3. how direct and interactive modes share implementation;
4. where parser tests and integration tests belong;
5. how to document a new command.

Keep all simulation semantics in the simulator/application services, not in terminal rendering or input code.

---

### 2.4 `make run-gui`

`make run-gui` must build the GUI target if necessary and launch the Dear ImGui workbench.

Expected use:

```bash
make run-gui
```

This replaces the distinction between the current `make gui` and `make run-gui`.

Required behavior:

1. configure the GUI-enabled build when necessary;
2. compile `cpssim_gui`;
3. launch `cpssim_gui`;
4. preserve current DPI handling and GUI behavior;
5. do not compile the complete test suite merely because the user launches the GUI, unless technically unavoidable during the first refactor.

The existing executable argument support may remain:

```bash
./build/<gui-build-dir>/cpssim_gui [config.json] [stop_tick] [--mock-functional]
```

Do not rename the executable unless there is a compelling reason.

---

### 2.5 `make test`

`make test` is the single public entry point for verification.

It should present an interactive verification menu when run by a user in a terminal:

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

#### Required modes

**Quick verification**

- formatting check only, without modifying files;
- normal Debug build;
- normal unit and integration tests.

**Test a specific module**

Offer test groups derived from CTest labels, not hard-coded Make targets. Initial useful groups include:

```text
core
config
kernel
scheduler
network
functional
fmi
bosch
conformance
gui
```

The exact set should reflect existing tests and may be refined.

**Run all tests**

- build the normal test configuration;
- run every registered normal test;
- show failures with `--output-on-failure`.

**Run full verification**

Run, in a clear sequence:

1. formatting check;
2. normal Debug build and all tests;
3. ASan/UBSan build and tests;
4. Release build and tests;
5. optionally Clang and clang-tidy if already supported and available.

A missing optional tool should produce a clear diagnostic. Do not silently report success when a requested verification step did not run.

**Formatting**

Present two explicit choices:

```text
1. Check formatting
2. Apply formatting
```

Normal tests must never modify source files. Formatting changes occur only after explicit user selection.

#### Non-interactive verification support

CI and code agents must not depend on terminal prompts.

Implement a script or verification driver that supports both interactive and direct operation, for example:

```bash
scripts/verify.sh
scripts/verify.sh quick
scripts/verify.sh all
scripts/verify.sh full
scripts/verify.sh module fmi
scripts/verify.sh format-check
scripts/verify.sh format-apply
```

Then:

```make
test:
	@./scripts/verify.sh
```

The exact implementation language may be POSIX shell or another lightweight option already acceptable for the supported Ubuntu environment. Avoid introducing a large dependency solely for the menu.

The non-interactive interface is required for:

- CI;
- code agents;
- reproducible developer instructions;
- pre-commit checks.

Adding a new test must not require a new Make target.

---

## 3. Test classification

Replace public test-selection Make targets with CTest labels.

Examples of current capabilities that should become labels or direct verification-driver modes:

- FMI lifecycle tests;
- online functional/replay tests;
- Bosch conformance;
- GUI-support tests;
- core/kernel/scheduler/network tests.

A test may have several labels. For example:

```text
Bosch online FMI execution:
  integration
  functional
  fmi
  bosch
```

Use CMake test properties to assign labels consistently.

The verification menu should derive its module choices from a maintained label list or queryable metadata. It must not know individual test executable implementation details unnecessarily.

The following existing public Make targets should be removed after equivalent verification-driver paths exist:

```text
conformance
fmi-test
functional-test
asan
release
format
format-check
```

The underlying executables, CMake presets, and test capabilities may remain.

---

## 4. Handling the existing Bosch executables

Current Bosch-specific Make targets:

```text
bosch-example
bosch-examples
```

must be removed from the public Make interface after `cpssim_cli run bosch` can invoke the equivalent behavior.

Do not delete validated Bosch execution code.

Refactor shared execution behavior out of `apps/bosch_example/main.cpp` into a reusable application-level service. Then:

- `cpssim_cli` calls the service;
- the legacy Bosch executable may temporarily call the same service;
- tests call lower-level APIs as appropriate.

After compatibility is confirmed, decide whether `cpssim_bosch_example` is still useful as an internal integration executable. Its existence must not require a public Make target.

---

## 5. Root Makefile target set

After the refactor, `.PHONY` and `make help` should expose only:

```text
all/default
run-cli
run-gui
test
clean
help
```

The default goal may be named internally, but users should invoke it as:

```bash
make
```

A conceptual root Makefile shape is:

```make
.DEFAULT_GOAL := all

.PHONY: all run-cli run-gui test clean help

all:
	cmake --preset <normal-preset>
	cmake --build --preset <normal-build-preset>

run-cli: all
	./build/<normal-build-dir>/cpssim_cli

run-gui:
	cmake --preset <gui-preset>
	cmake --build --preset <gui-build-preset> --target cpssim_gui
	./build/<gui-build-dir>/cpssim_gui

test:
	@./scripts/verify.sh

clean:
	cmake -E rm -rf <generated-build-directories>

help:
	@echo "CPSSim commands:"
	@echo "  make          Build CPSSim"
	@echo "  make run-cli  Launch the interactive terminal interface"
	@echo "  make run-gui  Launch the graphical workbench"
	@echo "  make test     Open the verification interface"
	@echo "  make clean    Remove generated build artifacts"
	@echo "  make help     Show this help"
```

Do not copy this verbatim without checking the repository's actual CMake preset names and generated paths.

Prefer generator-independent commands:

```bash
cmake --build --preset <preset>
```

instead of recursively calling `make -C <build-directory>`.

Do not perform a broad CMake-preset redesign unless needed to implement the public interface safely. Public-interface cleanup and internal preset cleanup may be separate commits.

---

## 6. Clean behavior

`make clean` must remove generated build products only.

It must not delete:

- source files;
- documentation;
- experiment configurations;
- Bosch reference artifacts;
- supplied examples;
- user-created run plans;
- saved experiment results, unless they live in an explicitly documented temporary output directory.

Document the exact directories removed.

---

## 7. Documentation updates

Update all documentation that currently recommends removed commands.

At minimum, search and revise:

```text
README.md
docs/COMMANDS.md
docs/gui/README.md
docs/guide/DEVELOPER-GUIDE.md
docs/guide/PROJECT-TOUR.md
docs/guide/FUTURE-WORK.md
docs/instructions/
```

Required documentation structure:

### README quick start

```bash
make
make run-cli
```

Graphical alternative:

```bash
make run-gui
```

Verification:

```bash
make test
```

### Command handbook

Explain:

- the six public Make commands;
- interactive CLI commands;
- direct non-interactive CLI examples;
- interactive and direct verification modes;
- how CTest labels classify tests;
- how developers add CLI commands;
- how developers add tests without modifying the Makefile.

Do not leave stale examples such as:

```text
make run
make gui
make bosch-example
make bosch-examples
make fmi-test
make functional-test
make conformance
make asan
make release
make format-check
```

unless a historical document explicitly marks them as obsolete.

---

## 8. Architecture constraints

Preserve the current CPSSim architectural boundaries.

1. The generic simulation core must not depend on:
   - Bosch-specific trigger definitions;
   - FMI implementation details;
   - terminal input/output;
   - Dear ImGui;
   - Make or shell scripts.

2. CLI and GUI are application interfaces over shared services.

3. Interactive prompts must not contain simulation semantics.

4. Direct and interactive CLI execution must produce equivalent simulation behavior for equivalent parameters.

5. Test menus must only select verification operations; they must not define test logic.

6. Presentation and terminal behavior must not change deterministic event ordering or canonical trace contents.

7. Avoid introducing a second Bosch execution implementation.

---

## 9. Implementation sequence

Use small, reviewable commits.

### Commit 1 — Public Make command surface

- add `run-cli`;
- retain or normalize `run-gui`;
- reduce `help`;
- keep old targets temporarily as undocumented aliases only if needed during migration;
- add deprecation messages if aliases are temporarily retained.

### Commit 2 — Verification driver

- add interactive/direct verification driver;
- map current verification commands into it;
- add CTest labels;
- switch `make test` to the new driver;
- remove old public verification targets.

### Commit 3 — CLI command framework

- replace the version-only CLI behavior;
- add interactive shell;
- add direct parser;
- implement `help`, `list`, `version`, `quit`, and `exit`;
- add parser and shell tests.

### Commit 4 — Bosch CLI integration

- extract reusable Bosch run service;
- implement direct `run bosch`;
- implement interactive Bosch wizard;
- verify behavior against existing Bosch references;
- remove public Bosch Make targets.

### Commit 5 — Documentation and cleanup

- update all guides;
- remove obsolete command references;
- decide whether temporary aliases can be deleted;
- run complete verification.

Do not combine all changes into one large commit.

---

## 10. Required tests

### Make interface

Verify:

```bash
make
make run-cli
make run-gui
make test
make clean
make help
```

Check that removed public targets are absent from help.

### CLI

Add tests for:

- empty interactive input;
- unknown command;
- `help`;
- `version`;
- `quit` and `exit`;
- direct Bosch argument parsing;
- invalid trajectory;
- invalid scenario;
- invalid stop tick;
- stop tick beyond available data;
- EOF handling;
- failed run returning a nonzero exit status.

### Interactive Bosch wizard

Test input/output logic independently from actual simulation execution by injecting input/output streams and a mock run service.

Do not make parser tests launch a real terminal.

### Verification driver

Test or manually validate:

```bash
scripts/verify.sh quick
scripts/verify.sh all
scripts/verify.sh full
scripts/verify.sh module fmi
scripts/verify.sh module bosch
scripts/verify.sh format-check
```

Unknown modes and labels must fail clearly.

### Regression

Run existing:

- normal tests;
- Bosch timing conformance;
- Bosch functional/reference tests;
- FMI lifecycle tests;
- GUI-support tests;
- ASan/UBSan tests;
- Release tests.

Existing canonical and functional reference behavior must remain unchanged.

---

## 11. Acceptance criteria

The task is complete only when all of the following hold:

1. `make help` lists only:
   - `make`;
   - `make run-cli`;
   - `make run-gui`;
   - `make test`;
   - `make clean`;
   - `make help`.

2. `make run-cli` launches a persistent CPSSim terminal interface.

3. The CLI supports both interactive and direct command execution.

4. The initial Bosch CLI path can run the three supplied examples using the two existing scenarios.

5. `make run-gui` launches the current GUI without weakening DPI behavior.

6. `make test` provides interactive verification selection.

7. The verification driver also supports non-interactive operation.

8. Existing specialized verification remains available without separate public Make targets.

9. Developers can add a CLI command without editing the Makefile.

10. Developers can add a test and classify it by labels without adding a Make target.

11. Documentation contains no stale normal-use command examples.

12. Existing deterministic traces and Bosch conformance results remain unchanged.

---

## 12. Non-goals for this refactor

Do not use this task to:

- redesign the simulator kernel;
- add new scheduling semantics;
- add rich network models;
- redesign the GUI layout;
- add workspace persistence;
- change Bosch FMU numerical behavior;
- change canonical event ordering;
- implement a plugin system;
- package CPSSim for Windows or macOS;
- introduce a large third-party CLI framework without clear justification.

Keep the refactor focused on command organization, CLI usability, test selection, and documentation.
