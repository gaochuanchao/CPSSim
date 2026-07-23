# Application and GUI Symbol Index

## Controller/session boundary

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `GuiCommandQueue::push/pop` | [`simulation_controller.hpp`](../../../src/cpssim/gui/simulation_controller.hpp) | [`simulation_controller.cpp`](../../../src/cpssim/gui/simulation_controller.cpp) | FIFO runtime commands |
| `SimulationController::SimulationController` | `simulation_controller.hpp` | `simulation_controller.cpp` | validate/copy inputs; create paused runtime |
| `SimulationController::enqueue` | `simulation_controller.hpp` | `simulation_controller.cpp` | queue control |
| `SimulationController::update` | `simulation_controller.hpp` | `simulation_controller.cpp` | consume commands and bounded progression |
| `SimulationController::snapshot` | `simulation_controller.hpp` | `simulation_controller.cpp` | detached copy |
| `SimulationController::reset` | private | `simulation_controller.cpp` | reconstruct model/policy/engine |
| `SimulationController::step_once` | private | `simulation_controller.cpp` | one complete logical tick |
| `GuiSimulationSession` | [`simulation_session.hpp`](../../../src/cpssim/gui/simulation_session.hpp) | [`simulation_session.cpp`](../../../src/cpssim/gui/simulation_session.cpp) | draft/active plan and controller replacement |
| `DraftRunPlan` | [`draft_run_plan.hpp`](../../../src/cpssim/gui/draft_run_plan.hpp) | [`draft_run_plan.cpp`](../../../src/cpssim/gui/draft_run_plan.cpp) | incomplete editable run fields |

## Project/application lifecycle

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `ProjectContext` | [`project.hpp`](../../../src/cpssim/application/project/project.hpp) | [`project.cpp`](../../../src/cpssim/application/project/project.cpp) | loaded project/session owner |
| `create_project` | `project.hpp` | `project.cpp` | atomic create |
| `load_project` | `project.hpp` | `project.cpp` | validate/construct replacement |
| `save_project` | `project.hpp` | `project.cpp` | persist applied values |
| `save_project_as` | `project.hpp` | `project.cpp` | copy/validate replacement |
| `WorkbenchApplication` constructors | [`workbench_application.hpp`](../../../src/cpssim/application/workbench_application.hpp) | [`workbench_application.cpp`](../../../src/cpssim/application/workbench_application.cpp) | non-rendering app owner |
| `initialize_system_draft` | `workbench_application.hpp` | `workbench_application.cpp` | draft from active input |
| `validate_system_draft` | `workbench_application.hpp` | `workbench_application.cpp` | canonical diagnostics |
| `apply_system_draft` | `workbench_application.hpp` | `workbench_application.cpp` | atomic active replacement |
| `enqueue/update` | `workbench_application.hpp` | `workbench_application.cpp` | active runtime progression |
| `replace_project` | `workbench_application.hpp` | `workbench_application.cpp` | publish project |
| `resolve_unapplied_changes` | `workbench_application.hpp` | `workbench_application.cpp` | apply-save/discard/cancel |
| `publish_complete_snapshot` | `workbench_application.hpp` | `workbench_application.cpp` | publication generation |
| `export_completed_result` | `workbench_application.hpp` | `workbench_application.cpp` | immutable export |

## Editable structural model

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `EditableSystemDraft` | [`editable_system_draft.hpp`](../../../src/cpssim/gui/editable_system_draft.hpp) | [`editable_system_draft.cpp`](../../../src/cpssim/gui/editable_system_draft.cpp) | detached editable system |
| `EditableSystemDraft::build` | `editable_system_draft.hpp` | `editable_system_draft.cpp` | canonical config/diagnostics |
| `add/duplicate/remove_resource` | `editable_system_draft.hpp` | `editable_system_draft.cpp` | resource rows |
| `add/duplicate/remove_task` | `editable_system_draft.hpp` | `editable_system_draft.cpp` | task rows |
| `set_execution_profile` | `editable_system_draft.hpp` | `editable_system_draft.cpp` | accessibility/demand |
| `add/set/remove_message_route` | `editable_system_draft.hpp` | `editable_system_draft.cpp` | links |
| `SystemExplorerInteraction` | [`system_builder_interaction.hpp`](../../../src/cpssim/gui/system_builder_interaction.hpp) | [`system_builder_interaction.cpp`](../../../src/cpssim/gui/system_builder_interaction.cpp) | create/duplicate/cascade/selection intent |
| `SystemBuilderWorkflow` | [`system_builder_workflow.hpp`](../../../src/cpssim/application/project/system_builder_workflow.hpp) | [`system_builder_workflow.cpp`](../../../src/cpssim/application/project/system_builder_workflow.cpp) | project-aware edit workflow |
| `ProjectSystemEditPolicy` | [`system_edit_policy.hpp`](../../../src/cpssim/application/project/system_edit_policy.hpp) | [`system_edit_policy.cpp`](../../../src/cpssim/application/project/system_edit_policy.cpp) | Generic/Bosch edit permissions |

## Selection and presentation

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `StructuralSelection` / `GuiSelection` | [`selection_model.hpp`](../../../src/cpssim/gui/selection_model.hpp) | [`selection_model.cpp`](../../../src/cpssim/gui/selection_model.cpp) | separate selection domains |
| `GuiArchitectureGraph` | [`architecture_graph.hpp`](../../../src/cpssim/gui/architecture_graph.hpp) | [`architecture_graph.cpp`](../../../src/cpssim/gui/architecture_graph.cpp) | detached nodes/links |
| architecture layout | [`architecture_layout.hpp`](../../../src/cpssim/gui/architecture_layout.hpp) | [`architecture_layout.cpp`](../../../src/cpssim/gui/architecture_layout.cpp) | deterministic positions |
| `GuiTimelineCache` | [`timeline_model.hpp`](../../../src/cpssim/gui/timeline_model.hpp) | [`timeline_model.cpp`](../../../src/cpssim/gui/timeline_model.cpp) | trace -> intervals |
| signal builders/cache | [`signal_series.hpp`](../../../src/cpssim/gui/signal_series.hpp) | [`signal_series.cpp`](../../../src/cpssim/gui/signal_series.cpp) | typed series/downsampling |
| event table model | [`event_table_model.hpp`](../../../src/cpssim/gui/event_table_model.hpp) | [`event_table_model.cpp`](../../../src/cpssim/gui/event_table_model.cpp) | filtering/cause lookup |
| resource presentation | [`resource_presentation.hpp`](../../../src/cpssim/gui/resource_presentation.hpp) | [`resource_presentation.cpp`](../../../src/cpssim/gui/resource_presentation.cpp) | utilization rows |
| workspace state | [`workspace_state.hpp`](../../../src/cpssim/gui/workspace_state.hpp) | [`workspace_state.cpp`](../../../src/cpssim/gui/workspace_state.cpp) | versioned presentation state |
| publication policy | [`presentation_publication.hpp`](../../../src/cpssim/gui/presentation_publication.hpp) | [`presentation_publication.cpp`](../../../src/cpssim/gui/presentation_publication.cpp) | generations/live gate |

## Qt bridge and structural controller

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `QtWorkbenchBridge` | [`workbench_bridge.hpp`](../../../apps/qt_gui/workbench_bridge.hpp) | [`workbench_bridge.cpp`](../../../apps/qt_gui/workbench_bridge.cpp) | sole Qt event-loop adapter |
| `QtStructuralEditController` | [`structural_edit_controller.hpp`](../../../apps/qt_gui/structural_edit_controller.hpp) | [`structural_edit_controller.cpp`](../../../apps/qt_gui/structural_edit_controller.cpp) | shared QUndoStack/mutations |
| `apply` | `structural_edit_controller.hpp` | `structural_edit_controller.cpp` | snapshot/mutate/undo command |
| `create_task` | `structural_edit_controller.hpp` | `structural_edit_controller.cpp` | stable task creation |
| `create_connection` | `structural_edit_controller.hpp` | `structural_edit_controller.cpp` | domain-backed link |
| `delete_connection` | `structural_edit_controller.hpp` | `structural_edit_controller.cpp` | domain-backed deletion |
| `synchronize_active_project` | `structural_edit_controller.hpp` | `structural_edit_controller.cpp` | root-scoped undo history |

## Qt views/models

| Component | Main files | Responsibility |
|---|---|---|
| main shell | [`main_window.hpp`](../../../apps/qt_gui/main_window.hpp), [`main_window.cpp`](../../../apps/qt_gui/main_window.cpp) | menus, docks, project transitions |
| Architecture model | [`architecture_model.hpp`](../../../apps/qt_gui/architecture_model.hpp), [`architecture_model.cpp`](../../../apps/qt_gui/architecture_model.cpp) | QtNodes adapter/ID mapping |
| Architecture view | [`architecture_view.hpp`](../../../apps/qt_gui/architecture_view.hpp), [`architecture_view.cpp`](../../../apps/qt_gui/architecture_view.cpp) | actions, context, scene interaction |
| Explorer | [`explorer_widget.hpp`](../../../apps/qt_gui/explorer_widget.hpp), [`explorer_widget.cpp`](../../../apps/qt_gui/explorer_widget.cpp) | structural tree |
| System Builder | [`system_builder_widget.hpp`](../../../apps/qt_gui/system_builder_widget.hpp), [`system_builder_widget.cpp`](../../../apps/qt_gui/system_builder_widget.cpp) | selected property pages |
| event table | [`event_table_widget.hpp`](../../../apps/qt_gui/event_table_widget.hpp), [`event_table_widget.cpp`](../../../apps/qt_gui/event_table_widget.cpp) | virtualized canonical events |
| runtime views | [`runtime_widgets.hpp`](../../../apps/qt_gui/runtime_widgets.hpp), [`runtime_widgets.cpp`](../../../apps/qt_gui/runtime_widgets.cpp) | resources/inspector |
| analysis views | [`analysis_widgets.hpp`](../../../apps/qt_gui/analysis_widgets.hpp), [`analysis_widgets.cpp`](../../../apps/qt_gui/analysis_widgets.cpp) | timeline/signals/results |
| assignment model | [`resource_assignment_model.hpp`](../../../apps/qt_gui/resource_assignment_model.hpp), [`resource_assignment_model.cpp`](../../../apps/qt_gui/resource_assignment_model.cpp) | read-only task-resource table |
| appearance | [`appearance_preferences.hpp`](../../../apps/qt_gui/appearance_preferences.hpp), [`workbench_style.cpp`](../../../apps/qt_gui/workbench_style.cpp) | global theme/style |

## Result/application symbols

| Symbol | Declaration | Implementation | Role |
|---|---|---|---|
| `derive_run_metrics` | [`run_result.hpp`](../../../src/cpssim/analysis/run_result.hpp) | [`run_result.cpp`](../../../src/cpssim/analysis/run_result.cpp) | deterministic metrics |
| `build_run_result` | `run_result.hpp` | `run_result.cpp` | immutable completed result |
| `CompletedRunFinalizer` | [`completed_run_finalizer.hpp`](../../../src/cpssim/analysis/completed_run_finalizer.hpp) | [`completed_run_finalizer.cpp`](../../../src/cpssim/analysis/completed_run_finalizer.cpp) | managed background derivation |
| `export_run_result` | [`result_export.hpp`](../../../src/cpssim/application/result_export.hpp) | [`result_export.cpp`](../../../src/cpssim/application/result_export.cpp) | atomic result publication |
| workbook writer | [`results_workbook.hpp`](../../../src/cpssim/application/results_workbook.hpp) | [`results_workbook.cpp`](../../../src/cpssim/application/results_workbook.cpp) | optional XLSX |
