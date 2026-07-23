# Troubleshooting

## Build cannot find Qt 6

Typical message:

```text
Could not find a package configuration file provided by Qt6
```

Install the Qt 6 development packages:

```bash
sudo apt install qt6-base-dev libgl1-mesa-dev
```

Then remove the affected build directory or rerun the preset.

## CMake cannot find Catch2

Install a compatible Catch2 3 package or provide it through the environment.
The project calls `find_package(Catch2 3 CONFIG REQUIRED)` for tests.

## First build fails while downloading QtNodes

The first GUI configuration fetches pinned source. Check:

- internet connectivity;
- TLS certificates;
- proxy settings;
- access to GitHub.

The error is normally in dependency acquisition, not CPSSim model validation.

## `make run-gui` launches an old build

Clean documented build directories:

```bash
make clean
make run-gui
```

Also confirm that you are in the intended repository clone.

## Project is invalid after adding a task

A new task normally needs:

1. valid name and timing;
2. at least one execution profile;
3. one resource assignment;
4. execution demand that satisfies validation.

Select the task in Explorer and complete its task page in System Builder.

## Resource is missing from assignment choices

An assignment can select only resources for which the task has an execution
profile. Add a profile first.

## Assignment resets or appears unassigned

Confirm that:

- the selected task still has a profile for that resource;
- the profile edit was committed;
- validation reports no duplicate/unknown ID;
- you are reading the task editor, not only the read-only assignment table.

## Communication delay is rejected

Current Generic Communication links allow zero or positive configured delay.
Logical links use zero delay and generate no messages. Confirm that the selected
link kind matches the intended semantics.

The fixed one-tick send offset is not edited as network latency.

## Delete key does not affect a link

Check that:

- the Architecture view or a synchronized structural view owns the current
  selection;
- the link is selected, not only visually hovered;
- the simulation is Paused rather than Running;
- the project policy permits structural edits.

Context-menu and Explorer delete should reach the same shared edit controller.

## Structural action is disabled

Structural creation, deletion, endpoint editing, and kind conversion are
disabled while Running. Pause or Reset first.

Bosch adapter-owned structure remains protected even while Paused.

## Save does not include draft changes

`Save Project` persists the applied system. Use **Validate changes** and
**Apply and restart** before saving. Close/replacement flows offer Apply and
Save, Discard, or Cancel when unapplied edits exist.

## Reset lost my edits

Reset reconstructs the active run from the last applied system and run plan. It
does not apply the pending draft. This is intentional.

## Result tab remains unavailable

Results are finalized only after the run reaches Finished. Pausing does not
produce final-run metrics.

Immediately after finishing, the tab may briefly show finalization in progress.

## Export refuses the destination

Common causes:

- the run ID already exists;
- the destination is not writable;
- no completed result exists;
- Selected Range was requested without a valid range;
- optional workbook creation failed.

The exporter intentionally refuses to overwrite an existing run directory.

## No message events appear for a link

Logical links do not generate network events. Use a Communication link.

For Communication links, messages are caused by accepted source `JobFinish`
events. A source job that never completes cannot publish its route.

## Message not delivered before stop tick

The horizon is inclusive but finite. A send or delivery after the stop tick is
not scheduled/processed. Inspect the message lifecycle; `PendingSend` or
`InFlight` may be the correct truncated final state.

## Timeline reports inconsistent lifecycle data

The timeline fails closed instead of inventing intervals. Inspect the reported
event sequence, tick, task, job, and resource. Preserve the trace and report the
smallest reproducing project.

## GUI appearance does not update

For a current Qt build, theme changes should repaint immediately. Confirm you
are launching `cpssim_gui`, not the legacy `cpssim_imgui_gui`, and rebuild the
current branch.

## DPI or monitor transition issue

Use the Qt frontend and current main branch. Record:

- source and destination monitor scale;
- operating system/session type;
- Qt version;
- whether hit testing or only rendering is affected;
- console assertion or stack trace.

Run focused GUI tests and a manual monitor-move smoke check; multi-monitor DPI
behavior cannot be fully proven by offscreen tests.

## Bosch FMU cannot load

Check:

- the prepared platform shared library exists;
- architecture matches the executable;
- required dynamic-library dependencies are installed;
- model identifier, GUID, and resource URI are correct;
- the selected trajectory contains enough valid samples.

FMI loader failures are reported separately from system/run-plan validation.

## Where to report a problem

Include:

```text
git rev-parse HEAD
operating system and compiler
Qt version if GUI-related
build preset
exact command
small project or input
full diagnostic / stack trace
whether quick/full verification passes
```
