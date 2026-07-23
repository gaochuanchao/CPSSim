# Test Index

Use the closest test as an executable behavior example. Test target names and
labels are defined in `CMakeLists.txt`; paths below are navigation anchors.

## Model and configuration

| Topic | Test |
|---|---|
| time conversion | [`tests/model/time_test.cpp`](../../../tests/model/time_test.cpp) |
| specifications | [`tests/model/specifications_test.cpp`](../../../tests/model/specifications_test.cpp) |
| runtime job/resource state | [`tests/model/runtime_state_test.cpp`](../../../tests/model/runtime_state_test.cpp) |
| run plans | [`tests/model/run_plan_test.cpp`](../../../tests/model/run_plan_test.cpp) |
| JSON configuration | [`tests/config/`](../../../tests/config/) |
| JSON run plan | [`tests/config/`](../../../tests/config/) |

## Kernel, policy, and network

| Topic | Test |
|---|---|
| event queue ordering | [`tests/kernel/event_queue_test.cpp`](../../../tests/kernel/event_queue_test.cpp) |
| periodic releases | [`tests/kernel/periodic_release_test.cpp`](../../../tests/kernel/periodic_release_test.cpp) |
| scheduler | [`tests/kernel/scheduler_test.cpp`](../../../tests/kernel/scheduler_test.cpp) |
| whole engine cycle | [`tests/kernel/simulation_engine_test.cpp`](../../../tests/kernel/simulation_engine_test.cpp) |
| fixed-priority policy | [`tests/policy/fixed_priority_test.cpp`](../../../tests/policy/fixed_priority_test.cpp) |
| resource allocation | [`tests/policy/`](../../../tests/policy/) |
| fixed-delay network | [`tests/network/fixed_delay_network_test.cpp`](../../../tests/network/fixed_delay_network_test.cpp) |

## Functional, FMI, Bosch, and conformance

| Topic | Test area |
|---|---|
| functional runtime/model | [`tests/functional/`](../../../tests/functional/) |
| FMI loader/lifecycle | [`tests/fmi/`](../../../tests/fmi/) |
| Bosch trigger/data/adapter | [`tests/bosch/`](../../../tests/bosch/) |
| captured timing/functional references | [`tests/conformance/`](../../../tests/conformance/) |
| captured experiment evidence | [`experiments/bosch_v10_reference/`](../../../experiments/bosch_v10_reference/) |

## GUI-independent support

| Topic | Test |
|---|---|
| command/snapshot controller | [`tests/gui/simulation_controller_test.cpp`](../../../tests/gui/simulation_controller_test.cpp) |
| system draft | [`tests/gui/editable_system_draft_test.cpp`](../../../tests/gui/editable_system_draft_test.cpp) |
| system builder interaction | [`tests/gui/system_builder_interaction_test.cpp`](../../../tests/gui/system_builder_interaction_test.cpp) |
| selection | [`tests/gui/selection_model_test.cpp`](../../../tests/gui/selection_model_test.cpp) |
| architecture graph/layout | [`tests/gui/architecture_graph_test.cpp`](../../../tests/gui/architecture_graph_test.cpp) |
| timeline derivation | [`tests/gui/timeline_model_test.cpp`](../../../tests/gui/timeline_model_test.cpp) |
| signals/downsampling | [`tests/gui/signal_series_test.cpp`](../../../tests/gui/signal_series_test.cpp) |
| workspace | [`tests/gui/workspace_state_test.cpp`](../../../tests/gui/workspace_state_test.cpp) |
| event table | [`tests/gui/event_table_model_test.cpp`](../../../tests/gui/event_table_model_test.cpp) |
| publication policy | [`tests/gui/presentation_publication_test.cpp`](../../../tests/gui/presentation_publication_test.cpp) |
| display scaling | [`tests/gui/display_scale_test.cpp`](../../../tests/gui/display_scale_test.cpp) |

## Application and analysis

| Topic | Test |
|---|---|
| project lifecycle | [`tests/application/`](../../../tests/application/) |
| workbench owner | [`tests/application/workbench_application_test.cpp`](../../../tests/application/workbench_application_test.cpp) |
| result export | [`tests/application/result_export_test.cpp`](../../../tests/application/result_export_test.cpp) |
| run metrics | [`tests/analysis/run_result_test.cpp`](../../../tests/analysis/run_result_test.cpp) |
| finalization | [`tests/analysis/completed_run_finalizer_test.cpp`](../../../tests/analysis/completed_run_finalizer_test.cpp) |
| CLI parser/shell/commands | [`tests/cli/`](../../../tests/cli/) |

## Qt frontend

| Topic | Test |
|---|---|
| main window/actions/lifecycle | [`tests/qt_gui/main_window_test.cpp`](../../../tests/qt_gui/main_window_test.cpp) |
| bridge/timers/publication | [`tests/qt_gui/`](../../../tests/qt_gui/) |
| structural edit controller | [`tests/qt_gui/structural_edit_controller_test.cpp`](../../../tests/qt_gui/structural_edit_controller_test.cpp) |
| Architecture model/interactions | [`tests/qt_gui/architecture_model_test.cpp`](../../../tests/qt_gui/architecture_model_test.cpp) |
| System Builder | [`tests/qt_gui/system_builder_widget_test.cpp`](../../../tests/qt_gui/system_builder_widget_test.cpp) |
| resource assignment model | [`tests/qt_gui/`](../../../tests/qt_gui/) |
| runtime/analysis widgets | [`tests/qt_gui/`](../../../tests/qt_gui/) |

## Module labels

Run:

```bash
./scripts/verify.sh list-modules
```

Maintained labels:

```text
core config kernel scheduler network functional fmi bosch conformance gui cli
```

Example:

```bash
./scripts/verify.sh module gui
```

## Risk-to-verification guide

| Change | Minimum before handoff |
|---|---|
| Markdown only | internal links + relevant quick test |
| isolated value/parser | focused unit + quick |
| ordering/lifecycle | unit + engine + conformance + full |
| ownership/async/FMI | focused + quick + ASan/UBSan + Release |
| Qt visual action | headless model + Qt integration + manual smoke |
| DPI/window behavior | Qt tests + real multi-monitor smoke |
| captured reference | independent regeneration record + full conformance |
