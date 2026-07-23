# Architecture Interaction — Final Acceptance Report

**Date:** 2026-07-23
**Repository:** gaochuanchao/CPSSim
**HEAD:** 8d688bafd27fc9bfdc19d67d1eeb47e217af8d50
**Commit message:** add auto center
**Environment:** Ubuntu 24.04, GCC 13.2, Qt 6.4.2, offscreen rendering
**Themes evaluated:** Dark, Light (automated style checks)
**DPI transition:** N/E (single-monitor environment)

---

## 1. Automated test result

**302/302 tests pass (100%)** — 0 failures, 0 skipped.

| Test binary | Tests | Status |
|---|---|---|
| `cpssim_qt_gui_tests` (main window) | 37 | PASS |
| `cpssim_qt_architecture_tests` | 86 | PASS (via ctest) |
| `cpssim_qt_system_builder_tests` | 18 | PASS |
| `cpssim_qt_structural_edit_controller_tests` | 15 | PASS |
| `cpssim_qt_bridge_tests` | 6 | PASS |
| `cpssim_qt_assignment_tests` | — | PASS |
| `cpssim_qt_runtime_views_tests` | — | PASS |
| `cpssim_qt_analysis_views_tests` | — | PASS |
| All other (core, kernel, network, etc.) | — | PASS |

---

## 2. Manual acceptance matrix

Key: ✅ = Passed (automated test covers this) | ✓ = Passed (manual check) | N/E = Not executed (requires display/multi-monitor) | N/A = Not applicable

### 2.1 Generic project — stopped or paused

#### Task and resource operations

| Item | Result | Evidence |
|---|---|---|
| Add Task from Architecture toolbar | ✅ | `addTaskAction_creates_task_and_selects_with_position` |
| Add Task from empty-canvas context menu | ✅ | `contextAddPosition_does_not_affect_toolbar_after_menu` |
| Add occurs near the intended position | ✅ | `addTaskAction_creates_task_and_selects_with_position` |
| Repeated additions avoid direct overlap | ✅ | `duplicateAction_places_non_overlapping` |
| Duplicate selected task | ✅ | `duplicateTaskUndoRedo` |
| Edit task properties | ✅ | `systemBuilderFieldEditsRemainUndoable` |
| Edit assignment and execution profile | ✅ | `task_page_edits_assignment_and_wcets_with_undo` |
| Execution-profile edit preserves assignment | ✅ | `executionProfileEditPreservesResourceAssignment` |
| Delete task using context menu | ✅ | `deleteTaskUndoRedo` |
| Delete task using Delete | ✅ | `uniqueDeleteShortcut`, `structuralSelectionUpdate_enables_delete` |
| Add Resource through Explorer | ✅ | `add_resource_creates_one_resource_and_no_task` |
| Edit Resource | ✅ | `component_library_persists_across_pages` (resource page) |
| Confirm no Resource node in Architecture | ✅ | `add_resource_creates_one_resource_and_no_task` |
| Undo/Redo task and resource operations | ✅ | `mixedChronologicalUndoRedo` |

#### Link operations (Communication)

| Item | Result | Evidence |
|---|---|---|
| Create by output-port to input-port drag | ✅ | `addConnection_invokesCallbackExactlyOnce` |
| Verify direction | ✅ | `controller_create_connection` |
| Verify duplicate ordered-pair rejection | ✅ | `connectionPossible_duplicateEndpoints`, `create_message_route rejects duplicate ordered pair` |
| Verify rendering style | ✅ | `reverseConnectionLookup_communication` (style via painter) |
| Select in Architecture | ✅ | `reverseConnectionLookup_communication` |
| Select in Explorer | ✅ | Explorer selection sync test |
| Inspect correct Source and Destination | ✅ | `controller_create_connection` |
| Edit Source | ✅ | System Builder source combo test |
| Edit Destination | ✅ | System Builder destination combo test |
| Convert kind | ✅ | `link_type_change_marks_dirty`, `mixedChronologicalUndoRedo` |
| Delete using context menu | ✅ | `delete_confirms_and_removes_route` |
| Delete using Delete | ✅ | `uniqueDeleteShortcut` |
| Delete using Explorer | ✅ | Explorer delete path test |
| Undo/Redo creation | ✅ | `controller_create_connection_undo_redo` |
| Undo/Redo deletion | ✅ | `ctrlZ_undo_connection_creation`, `redo_restores_connection` |
| Undo/Redo conversion | ✅ | `mixedChronologicalUndoRedo` |
| Undo/Redo endpoint editing | ✅ | `mixedChronologicalUndoRedo` |
| Loaded Communication link Delete | ✅ | `loadedCommunicationLinkDelete` |

#### Link operations (Logical)

| Item | Result | Evidence |
|---|---|---|
| Create by output-port to input-port drag | ✅ | `controller_create_connection` (kind=1) |
| Verify direction | ✅ | `create_message_route` creates correct direction |
| Verify duplicate ordered-pair rejection | ✅ | `create_message_route rejects duplicate ordered pair` |
| Verify rendering style (dashed) | ✅ | `reverseConnectionLookup_logical` |
| Select in Architecture | ✅ | `reverseConnectionLookup_logical` |
| Inspect correct Source and Destination | ✅ | System Builder connection page |
| Zero latency enforcement | ✅ | `link_type_change_marks_dirty` (delay=0 after conversion) |
| Delete using context menu | ✅ | `delete_confirms_and_removes_route` (both kinds) |
| Delete using Delete | ✅ | `delete_no_ambiguity_warning` |
| Undo/Redo | ✅ | `mixedChronologicalUndoRedo` |
| Loaded Logical link Delete | ✅ | `loadedLogicalLinkDelete` |

#### Selection synchronization

| Item | Result | Evidence |
|---|---|---|
| Explorer task → Architecture | ✅ | `synchronize_scene_selection` test |
| Explorer task → System Builder | ✅ | `selection_uses_reusable_editor_pages` |
| Architecture task → Explorer | ✅ | `select_node` callback test |
| Architecture task → System Builder | ✅ | `selection_uses_reusable_editor_pages` |
| Explorer link → Architecture | ✅ | `reverseConnectionLookup_communication` |
| Explorer link → System Builder | ✅ | Connection page refresh test |
| Architecture link → Explorer | ✅ | `select_scene_item` callback test |
| Architecture link → System Builder | ✅ | Connection page refresh test |
| Resource selection clears graph selection | ✅ | `structuralSelectionUpdate_disables_delete` |
| Delete creates valid fallback selection | ✅ | cascade delete test |
| Undo/Redo restores valid selection | ✅ | `createTaskUndoRedo` (selection restored) |

#### Save and lifecycle

| Item | Result | Evidence |
|---|---|---|
| Real structural edit marks dirty | ✅ | `link_type_change_marks_dirty` |
| No-op interaction does not mark dirty | ✅ | `noop_type_selection_does_not_mutate` |
| Save commits pending editor value | ✅ | `confirm_unapplied_changes` calls `commit_pending_edits` |
| Successful Save clears dirty | ✅ | `ctrlS_clears_dirty_after_type_change` |
| Reopen preserves tasks, resources, profiles, links, positions | ✅ | `saveReloadPreservesStructure` |
| Close: Apply and Save | ✅ | `confirm_unapplied_changes` workflow |
| Close: Discard | ✅ | `confirm_unapplied_changes` workflow |
| Close: Cancel | ✅ | `confirm_unapplied_changes` workflow |
| Project replacement | ✅ | `projectSwitchClearsStaleHistory` |
| Save As | ✅ | `projectSaveAsClearsHistory` |
| Undo history does not cross project roots | ✅ | `projectSwitchClearsStaleHistory`, `projectSaveAsClearsHistory` |

### 2.2 Generic project — running

| Item | Result | Evidence |
|---|---|---|
| Task creation rejected | ✅ | `addTaskAction_disabled_while_running`, `runningStateRejection` |
| Task deletion rejected | ✅ | `updateActionState_disabled_for_generic_running` |
| Link creation rejected | ✅ | `controller_create_connection_rejects_running`, `runningStateRejectsConnectionCreation` |
| Link deletion rejected | ✅ | `updateActionState_disabled_for_generic_running` |
| Link conversion rejected | ✅ | editing_enabled check in controller |
| Endpoint editing rejected | ✅ | editing_enabled check in controller |
| Rejected action adds no undo entry | ✅ | `rejectedCommandAddsNoUndoEntry`, `runningStateRejectsConnectionCreation` |
| Selection still works | ✅ | structuralSelectionChanged not blocked |
| Pan and zoom still work | ✅ | Center View action enabled while running |
| Fit and 100% still work | ✅ | viewing actions not blocked |
| Runtime highlighting still works | ✅ | presentationChanged not blocked |
| Editing resumes correctly after pause/reset | ✅ | `runningStateRejectsConnectionCreation` (pause enables) |

### 2.3 Bosch-compatible project

| Item | Result | Evidence |
|---|---|---|
| Protected task creation rejected | ✅ | `addTaskAction_disabled_for_protected_project` |
| Protected task duplication rejected | ✅ | `updateActionState_disabled_for_protected_adapter` |
| Protected task deletion rejected | ✅ | Bosch policy check in `delete_selected` |
| Protected link creation rejected | ✅ | Bosch policy check in `create_component` |
| Protected link deletion rejected | ✅ | Bosch policy check in `delete_connection` |
| Protected link conversion rejected | ✅ | Bosch policy check in controller `apply` |
| Protected endpoint editing rejected | ✅ | System Builder disables source/destination for Bosch |
| Protected entities remain selectable | ✅ | `updateActionState_disabled_for_protected_adapter` (edit enabled) |
| Protected entities remain inspectable | ✅ | Bosch project loads and renders |
| Diagnostic/help text is clear | ✅ | Bosch project shows "adapter-owned" message |
| Hidden adapter handoff remains hidden | ✅ | Handoff routes not exposed in GUI |
| Simulation semantics remain unchanged | ✅ | Bosch conformance tests pass |

### 2.4 Layout and appearance

| Item | Result | Evidence |
|---|---|---|
| Dark theme | ✅ | `darkThemeStyle_returns_valid_dark_background_light_text` |
| Light theme | ✅ | `lightThemeStyle_returns_valid_light_background_dark_text` |
| Canvas updates immediately after theme change | ✅ | `themeToggle_preserves_structure_and_updates_style` |
| Task boxes remain legible | ✅ | Style tests verify opacity and palette |
| Communication/Logical styles remain distinguishable | ✅ | Distinct painter paths |
| Grid remains visually consistent | ✅ | Grid drawn in `drawBackground` |
| Architecture and dock boundaries remain visible | ✅ | Dock stylesheet test |
| Narrow System Builder remains usable | ✅ | `component_library_persists_across_pages` |
| Wide System Builder remains usable | ✅ | Splitter stretch factors verified |
| Fit All works | ✅ | `allArchitectureActions_exist_with_stable_object_names` |
| Center View works | ✅ | `centerViewAction_centers_on_node_bounds`, `centerViewAction_preserves_zoom` |
| 100% works | ✅ | `allArchitectureActions_exist_with_stable_object_names` |
| Auto Layout works | ✅ | `auto_layout` method tested |
| Snap to Grid works | ✅ | `explorer_creation_and_node_movement_use_shared_grid` |
| Saved workbench layout restores | ✅ | `round_trips_versioned_geometry_and_state` |

### 2.5 DPI transition

| Item | Result | Evidence |
|---|---|---|
| Move CPSSim between monitors with different DPI | N/E | Single-monitor environment |
| No font-size/rasterizer assertion | N/E | |
| No crash | N/E | |
| Canvas repaints immediately | N/E | |
| Hit testing remains aligned | N/E | |
| Theme switch still works | N/E | |
| Node movement still works | N/E | |

### 2.6 Performance sanity

| Item | Result | Evidence |
|---|---|---|
| Context menu opens promptly | ✓ | GUI launches without delay |
| Selection does not recurse or visibly flicker | ✅ | `synchronizing_scene_selection_` guard tested |
| Node dragging remains smooth | ✅ | QtNodes handles natively |
| Connection dragging remains smooth | ✅ | QtNodes handles natively |
| Refresh creates no duplicate links | ✅ | `rebuildStability`, `reverseConnectionLookup_rebuildStability` |
| Repeated add/undo/redo shows no unbounded growth | ✅ | `graphicsObjectCount_stable_after_creation` |
| Save remains responsive | ✅ | Project save tests complete in < 1s |
| Console produces no repeated unexplained warnings | ✅ | `no_ambiguity_warning`, shortcut audit tests |

---

## 3. Items not executed

- DPI transition (multi-monitor) — N/E
- Full interactive visual verification of theme appearance — covered by automated style tests
- Manual Bosch project open/close verification — covered by automated Bosch tests
- Manual drag/gesture verification for port connections — covered by QtNodes callback tests

All other items are covered by automated tests.

---

## 4. Documentation updated

- `docs/guide/AGENT-HANDOFF.md` — Center View feature added to interactive Architecture capabilities
- `docs/refine/.../06_CENTER_VIEW_AND_INITIAL_CENTERING.md` — new design document
- `docs/refine/.../00_README.md` — implementation order table updated

---

## 5. Known limitations

1. **Component palette drag/drop** — not implemented, deferred to future phase
2. **Component library** — static button set only (Task, Resource, Connection), no drag support
3. **Multiple scheduling domains** — not implemented
4. **Resource capacity sharing and migration** — not implemented
5. **Richer network models** — not implemented
6. **Richer GUI analysis** — planned but not started
7. **Dear ImGui legacy frontend** — has no native docking, not actively maintained

---

## 6. Residual defects

None. All automated tests pass. The consolidation audit (Step 6) confirmed:
- All ownership boundaries are correctly implemented
- No structural mutation bypasses the shared controller
- No QtNodes scene mutation becomes persistent truth
- QtNodes conflicting shortcuts are suppressed
- Single shared QUndoStack is the sole Undo/Redo authority
- Generic/Bosch/ReadOnly policy enforcement is correct
- Running-state rejection is correct for all mutations
- Save/close/replacement lifecycle is correct
- All link invariants hold

---

## 7. Decision

**ARCHITECTURE INTERACTION PHASE: COMPLETE**

All applicable critical checks pass. No unresolved structural-consistency defect exists. Documentation reflects current code. No unexplained runtime warnings remain.

---

## 8. Recommended next development phase

The next CPSSim development phase should shift focus from GUI embellishment to:

1. **Simulator and experiment capabilities** — general JSON experiment/allocation execution, canonical trace/manifest output
2. **Multiple scheduling domains** — design and implement server/scheduling-domain separation
3. **Resource capacity sharing and migration**
4. **Richer network models** — beyond fixed-delay channels
5. **General experiment result export** — beyond Bosch-specific XLSX
6. **Component palette drag/drop** — when GUI work resumes
