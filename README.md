# CPSSim

CPSSim is a portable, deterministic, event-driven C++ simulator for studying
real-time cyber-physical systems. It separates a generic simulation core from
scenario adapters, scheduling policies, functional models, and visualization.

The first validation scenario is the Bosch Physics-Driven Real-Time CPS
Challenge. CPSSim is now developed as a standalone repository rather than as a
continuously synchronized fork of the Bosch challenge repository.

## Current status

The project currently provides:

- a C++20 CMake project with Debug, Release, sanitizer, Clang, and clang-tidy
  workflows;
- strong identifier types;
- signed integer simulation ticks and exact physical-duration conversions;
- event and job-lifecycle categories;
- validated immutable resources, resource-independent tasks, and per-resource
  execution profiles;
- a strict, versioned JSON experiment configuration format with an explicit
  preemptive/non-preemptive scheduling assumption and generic message routes;
- runtime `Task` and `Resource` objects plus validated job lifecycle transitions;
- a portable canonical event record with deterministic versioned JSON Lines
  serialization;
- a deterministic pending-event queue ordered by integer tick, explicit phase,
  and stable insertion sequence;
- task-level resource assignment and task-driven job generation with offsets,
  an inclusive horizon, one pending release per task, and task-local job IDs;
- separate task-placement and job-ordering policy interfaces, with a
  single-resource allocator, explicitly configured allocator, and
  fixed-priority policy;
- a runtime scheduler that owns jobs, independent per-resource Ready queues,
  resources, and dispatch/preemption mechanism;
- an event-driven multi-resource engine focused on deterministic global time,
  event routing, release progression, and processed canonical traces;
- generic completion-triggered messages with deterministic runtime IDs,
  positive fixed send/delivery timing, causal trace links, and inspectable
  horizon-truncated lifecycle;
- a separate Bosch trigger adapter with explicit sixteen-input mapping and
  deterministic sparse CSV export;
- a captured-reference conformance tool whose dedicated and shared-cloud
  scheduler, network, and trigger streams match exactly;
- a core-independent FMI 2.0 Co-Simulation adapter with dynamic loading,
  lifecycle ownership, typed value-reference access, stepping, explicit
  statuses, and real Bosch-source execution tests on Linux;
- a generic functional-model boundary with typed online observations, a
  policy observation hook, append-only functional traces, and offline replay;
- a Bosch FMI functional adapter whose complete initialization, trajectory,
  trigger, and output mappings reproduce both 150,001-row references within
  documented numerical tolerances;
- a strict Bosch example loader and executable that run the supplied 10, 12.5,
  and 15 m/s trajectory formats through the normal engine and real FMU;
- resumable next-event-tick engine progression plus a GUI-neutral FIFO command
  and detached snapshot boundary;
- an optional DPI-aware Dear ImGui/GLFW/OpenGL workbench with a validated
  run-plan editor, shared selection, architecture graph, scheduling timeline,
  typed functional plots, canonical events, and resource/runtime inspection;
- Catch2 unit tests; and
- captured MATLAB/Simulink references for trigger, replay, and later numerical
  conformance work.

The initial fixed-delay network, online functional interaction, and GUI
workbench are implemented. Rich networking (payloads, contention, loss, and
random delay), broader FMI packaging, workspace persistence, and advanced
visualization remain later roadmap work. Resources currently behave as
independent exclusive uniprocessors without global scheduling or migration.

## Build and test

On the supported Ubuntu development environment:

```bash
make test
```

Useful completion checks are:

```bash
make release
make asan
make format-check
```

See the [command handbook](docs/COMMANDS.md) for the complete terminal
workflow. The [documentation home](docs/README.md) provides short paths for
learning the architecture, exact simulation behavior, code-reading order,
testing, and extension work.

## Documentation

Start with the [documentation home](docs/README.md), then choose the
[project tour](docs/guide/PROJECT-TOUR.md),
[simulation semantics](docs/guide/SIMULATION-SEMANTICS.md), or
[developer guide](docs/guide/DEVELOPER-GUIDE.md) according to your goal.
For visual experiment work, use the [GUI tutorial](docs/gui/README.md).
Improvements that have been discussed but not implemented are separated by
status and proposed implementation path in the
[future-work guide](docs/guide/FUTURE-WORK.md).

Stable project direction is defined by the
[project charter](docs/instructions/00_PROJECT_CHARTER.md),
[architecture](docs/instructions/01_ARCHITECTURE.md), and
[roadmap](docs/instructions/02_ROADMAP.md).

Durable design decisions are recorded under [`docs/adr/`](docs/adr/). Current
behavior and validation guidance stay in the guides, module pages, tests, and
experiment documentation; Git retains chronological history.

## Bosch and MATLAB provenance

CPSSim originated from the
[Bosch CPS Challenge repository](https://github.com/boschresearch/CPSChallenge).
The supplied FMU, Simulink model, example data, paper, presentation, and related
materials retain their original authorship and license notices.

The original Bosch root README is preserved as
[BOSCH_CHALLENGE_README.md](resources/BOSCH_CHALLENGE_README.md). Earlier
MATLAB-first development is retained on the
[`matlab` branch](https://github.com/gaochuanchao/CPSSim/tree/matlab), while the
captured behavior oracle is documented under
[`experiments/bosch_v10_reference/`](experiments/bosch_v10_reference/).

The generic simulator core must not depend on Bosch trigger definitions,
MATLAB, Simulink, FMI, or GUI libraries.

## License

See [LICENSE.txt](LICENSE.txt). Individual supplied resources also retain the
copyright and attribution stated in their accompanying documentation.
