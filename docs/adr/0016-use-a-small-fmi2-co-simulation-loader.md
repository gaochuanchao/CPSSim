# ADR-0016: Use a Small FMI 2.0 Co-Simulation Loader

- Status: Accepted
- Date: 2026-07-19
- Owners: Chuanchao Gao
- Related tasks: T15, T16

## Context

T15 must load and drive an FMI 2.0 Co-Simulation model from C++ while keeping
FMI out of `cpssim_core`. The initial research prototype needs a small subset
of the importer role:

- load one platform shared library;
- resolve the required FMI 2.0 symbols;
- instantiate, initialize, step, terminate, and free one component;
- get and set Real, Integer, Boolean, and String values by value reference; and
- propagate FMI status and adapter errors without silently continuing.

The official FMI site describes an FMU as a ZIP container holding XML plus
binary or C implementations and lists FMI 2.0.5 as the current FMI 2 release:
[FMI project](https://fmi-standard.org/).

The supplied `LateralMotionControl.fmu` contains a Win64 DLL and complete C
source, but no Linux binary. Its tracked source can therefore be compiled into
a Linux shared library for tests without changing the archived golden FMU.

## Options considered

| Option | Advantages | Costs for CPSSim T15 |
|---|---|---|
| [FMI Library 3.0.4](https://github.com/modelon-community/fmi-library/releases/tag/3.0.4) | Mature C importer; archive, XML, FMI 2, and FMI 3 support | Much larger API and dependency/build surface; archive/XML capabilities exceed the current task |
| [FMI4cpp](https://github.com/NTNU-IHB/FMI4cpp) | Convenient modern C++ FMI 2 API; archive and model-description handling | Project describes itself as work in progress; requires `libzip` and `pugixml`; adds abstractions not otherwise used by CPSSim |
| [FMPy](https://github.com/CATIA-Systems/FMPy) | Maintained, feature-rich, and able to compile source-code FMUs | Python runtime and scientific Python dependencies conflict with the standalone C++ runtime goal |
| Small CPSSim loader | Minimal dependency surface; inspectable lifecycle and error behavior; direct fit for one Co-Simulation FMU | CPSSim owns dynamic-loading code; no general archive/XML support in T15 |

These libraries remain useful validation and future-extension references. The
decision is not a claim that a custom loader is preferable for a general FMI
product.

## Decision

T15 implements a small `cpssim_fmi2_adapter` library. It depends on platform
dynamic-loading facilities and the C++ standard library only. `cpssim_core`
does not depend on it.

The public adapter receives a prepared model description containing:

- platform shared-library path;
- FMI model identifier used as the exported-symbol prefix;
- GUID;
- resource URI; and
- instance name.

Archive extraction and arbitrary `modelDescription.xml` parsing are not part
of T15. A later packaging layer may provide those values without changing the
runtime lifecycle interface.

### Lifecycle

The adapter owns these states:

```text
Loaded -> Initialized -> Terminated
```

Construction opens the shared library and resolves every required symbol.
`initialize()` performs instantiate, setup experiment, enter initialization,
and exit initialization in the FMI-defined order. `do_step()` and typed value
access require `Initialized`. `terminate()` calls the FMU and then frees the
component. Destruction performs best-effort termination/freeing and always
closes the library.

Construction failures throw because no usable adapter object can be created.
Runtime calls return an explicit `Fmi2CallResult` containing the FMI status and
diagnostic message. FMI `OK` and `Warning` mean the call completed; `Discard`,
`Error`, `Fatal`, and `Pending` are preserved for the caller.

### Time boundary

The FMI API requires floating-point seconds. Those values exist only at this
adapter boundary. T16 must derive communication points and step sizes from
canonical integer ticks rather than accumulating floating-point time.

### Linux Bosch test model

CMake extracts no replacement golden artifact. It compiles the already
tracked Bosch C source as a build-tree-only Linux `.so`. Tests load that file
through the same runtime importer used for other prepared FMI 2.0 shared
libraries. The original `.fmu` and Win64 DLL remain unchanged.

## Consequences

Positive:

- lifecycle, symbol, status, and cleanup behavior stays visible and testable;
- no new runtime package is required for the initial Ubuntu prototype;
- FMI remains outside the generic simulation core;
- the real Bosch model executes in Linux tests; and
- a future library-backed implementation can preserve the public adapter
  boundary if archive/XML requirements grow.

Limiting:

- callers must prepare/extract the FMU and supply model metadata;
- T15 supports FMI 2.0 Co-Simulation only, not Model Exchange or FMI 3;
- variable lookup is by typed value reference, not XML name;
- only one component is owned per adapter instance; and
- Windows/macOS loading paths require platform testing beyond the current
  Ubuntu validation.

## Reconsideration triggers

Reevaluate FMI Library or FMI4cpp when CPSSim needs general `.fmu` extraction,
schema-complete model-description parsing, FMI 3, Model Exchange, dependency
binary discovery, or broad third-party FMU compatibility.
