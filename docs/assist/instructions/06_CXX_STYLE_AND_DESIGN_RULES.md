# C++ Style and Design Rules

## 1. Language and build

- C++20
- CMake
- out-of-source builds
- warnings enabled
- warnings treated as errors for project code
- debug builds with assertions
- sanitizer presets when supported

## 2. Ownership

- Use RAII.
- Prefer values and `std::unique_ptr`.
- Use `std::shared_ptr` only when shared lifetime is real and documented.
- Do not expose owning raw pointers.
- Do not store references to objects whose lifetime is not guaranteed.

## 3. Types

Project declarations use the single `cpssim` C++ namespace. Do not mirror
source folders with nested namespaces such as `cpssim::model` or
`cpssim::config`. Choose unique, descriptive project-wide type and function
names instead.

Prefer strong types for:

- task ID;
- job ID;
- resource ID;
- message ID;
- vehicle ID;
- event sequence;
- simulation tick.

Avoid mixing identifiers and ordinary integers accidentally.

Use `enum class` for:

- event type;
- job state;
- resource type;
- command type;
- event phase.

### Prefer the simplest sufficient syntax

This project is a research prototype and a learning project. Start with the
smallest C++ construct that expresses the behavior and invariants required by
the current task.

Do not add annotations or special operations speculatively. Features such as
`[[nodiscard]]`, `noexcept`, `constexpr`, comparison operators, and fixed enum
storage types should be introduced only when a current interface, invariant,
test, serialization format, or measured performance need requires them.
Explain that reason in the task documentation.

Prefer a plain aggregate `struct` for a record whose fields are intended to be
public. A small class and constructor are appropriate when they enforce a
current invariant, such as preventing an identifier from being modified or
silently created from a raw integer. Continue to use `enum class`, fixed-width
integer types, `const` local values, validation, and error handling where they
directly express project semantics. Readability does not override deterministic
timing, type separation, state ownership, or other architectural constraints.

The clang-tidy `performance-enum-size` check is disabled. It recommends a fixed
underlying type for small enums to reduce storage, but enum size is not a
measured bottleneck in the prototype. Specify an enum representation when an
external format, ABI, or measured memory requirement makes it part of the
design.

## 4. Data and behavior

Keep immutable specifications separate from mutable state.

Example:

```cpp
struct PeriodicTimingSpec {
    Tick period;
    Tick deadline;
    Tick offset;
};

class TaskSpec {
    TaskId id;
    PeriodicTimingSpec timing;
    Priority priority;
};

struct TaskResourceProfile {
    TaskId task_id;
    Tick execution_time;
    ResourceId resource;
};

struct JobState {
    JobId id; // Task-local job number; pair with task_id for complete identity.
    TaskId task_id;
    Tick release_tick;
    Tick absolute_deadline;
    Tick remaining_execution;
    JobLifecycle lifecycle;
};
```

Do not mutate `TaskSpec` to store the current job.

## 5. Interfaces

Keep interfaces small.

A scheduling policy should return a decision, not manipulate the resource:

```cpp
ScheduleDecision decide(const SchedulingView& view);
```

An adapter should translate, not redefine core semantics.

## 6. Containers and determinism

Do not rely on unspecified iteration order.

When iteration order affects results:

- use ordered containers;
- sort explicitly;
- define a stable comparator;
- test the order.

## 7. Error handling

Use:

- exceptions for unrecoverable configuration or construction errors when
  appropriate;
- expected/result-style return values for runtime adapter failures;
- assertions for violated internal invariants.

Do not silently ignore FMI, file, or configuration errors.

## 8. Logging

Separate:

- canonical simulation trace;
- diagnostic application log;
- GUI display log.

The canonical trace is part of simulation output and must remain deterministic.
Diagnostic logging may include wall-clock information but must not affect the
trace.

## 9. Performance

Do not optimize the first implementation prematurely.

Before introducing custom allocators, lock-free queues, object pools, or
parallel execution:

1. establish a benchmark;
2. measure the bottleneck;
3. document the result;
4. create an ADR if the change affects architecture.

## 10. Formatting

Adopt `clang-format` and `clang-tidy` configurations early.

The exact style can be selected during project bootstrap, but the repository
must apply one consistent automated format.

## 11. Source-file and function descriptions

Every project-owned `.hpp` and `.cpp` file under `src/`, `apps/`, and `tests/`
starts with a `/*** ... ***/` block before `#pragma once` or the first
`#include`. The block records:

- the repository-relative file name;
- the file's responsibility;
- `Creator: Chuanchao Gao`;
- the date on which the description was established or materially revised;
- important architectural boundaries or reading notes when needed.

The creator field identifies the CPSSim project creator requested for these
source headers. Git history remains the authoritative record of who made each
individual edit.

Use plain `//` for a short one-line explanation inside a class or function.
This includes simple getters, small constructors, comparison operations, and
short implementation notes. CPSSim does not currently generate Doxygen output,
so `///` is not used merely to make a one-line comment look more formal.

Use a `/*** ... ***/` block for:

- every source-file header;
- descriptions before a class or struct;
- descriptions before a top-level function or test case; and
- any description that requires multiple lines.

For behavior-bearing functions, explain the relevant input, result,
exceptions, state changes, and invariants.

When a function is declared in a header and defined in a `.cpp` file:

- the header description is the public contract for callers;
- the implementation description explains how validation or coordinated state
  changes satisfy that contract.

This avoids copying the same long text into two places while still making both
files understandable when read independently. Comments must be updated in the
same change when behavior or an interface changes. Generated files and
third-party dependencies are excluded.
