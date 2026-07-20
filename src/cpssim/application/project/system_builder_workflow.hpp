/*** Declare atomic project rebuild and unapplied-change decision services. ***/

#pragma once

#include "cpssim/gui/application_state.hpp"
#include "cpssim/gui/editable_system_draft.hpp"

#include <memory>
#include <string>
#include <vector>

namespace cpssim {

struct SystemProjectRebuildResult {
    std::unique_ptr<ProjectContext> replacement;
    std::vector<SystemDraftDiagnostic> system_diagnostics;
    std::vector<RunPlanDiagnostic> run_plan_diagnostics;
    std::string diagnostic;
    bool applied{false};

    bool valid() const { return replacement != nullptr || applied; }
};

/*** Builds every immutable/runtime replacement component without changing application state. ***/
SystemProjectRebuildResult
build_system_project_replacement(const ProjectContext& current, const EditableSystemDraft& draft,
                                 const std::vector<DraftTaskAssignment>* assignments = nullptr);

/*** Replaces the active project only after complete system/session reconstruction succeeds. ***/
SystemProjectRebuildResult
apply_system_project_draft(GuiApplicationState& state, const EditableSystemDraft& draft,
                           const std::vector<DraftTaskAssignment>* assignments = nullptr);

enum class UnappliedSystemDecision {
    ApplyAndSave,
    Discard,
    Cancel,
};

enum class ProjectTransitionStatus {
    Proceed,
    Cancelled,
    Failed,
};

struct ProjectTransitionResult {
    ProjectTransitionStatus status{ProjectTransitionStatus::Cancelled};
    std::string diagnostic;
};

/*** Resolves the draft before a caller closes or replaces the active project. ***/
ProjectTransitionResult
resolve_unapplied_system_changes(GuiApplicationState& state, const EditableSystemDraft* draft,
                                 UnappliedSystemDecision decision,
                                 const std::vector<DraftTaskAssignment>* assignments = nullptr);

} // namespace cpssim
