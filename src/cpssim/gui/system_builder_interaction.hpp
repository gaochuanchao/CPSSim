/*** Declare headless Explorer lifecycle actions for the detached system draft. ***/

#pragma once

#include "cpssim/gui/draft_run_plan.hpp"
#include "cpssim/gui/editable_system_draft.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim {

enum class SystemBuilderFocusTarget {
    None,
    ResourceName,
    TaskName,
    ProfileExecutionTime,
    RouteSource,
};

struct SystemExplorerActionResult {
    bool changed{false};
    std::optional<StructuralSection> expand_section;
    std::optional<StructuralSelection> scroll_to;
    SystemBuilderFocusTarget focus{SystemBuilderFocusTarget::None};
    std::string diagnostic;
};

struct SystemEntityCreateAvailability {
    bool available{false};
    std::string explanation;
};

struct SystemDeletionImpact {
    StructuralSelection target;
    SystemDraftCascadeImpact structural;
    std::size_t run_plan_assignments{0};
};

/*** Lightweight result for a single connection-creation attempt. ***/
struct SystemBuilderInteractionResult {
    bool changed{false};
    std::string diagnostic;
};

/***
 * Creates one message route between explicit task endpoints in the detached
 * draft, selects it, and returns a diagnostic on failure.
 *
 * Validates: source exists, destination exists, duplicate ordered pair,
 * self-loop per current domain policy.
 *
 * Graphics-independent — emits no Qt signals.
 ***/
SystemBuilderInteractionResult
create_message_route(TaskId source, TaskId destination, EditableSystemDraft& draft,
                     StructuralSelection& selection);

/*** Owns pending confirmation while all mutations remain in caller-owned drafts. ***/
class SystemExplorerInteraction {
  public:
    SystemEntityCreateAvailability create_availability(StructuralSection section,
                                                       const EditableSystemDraft& draft) const;

    SystemExplorerActionResult create(StructuralSection section, EditableSystemDraft& draft,
                                      StructuralSelection& selection);
    SystemExplorerActionResult duplicate(const StructuralSelection& target,
                                         EditableSystemDraft& draft,
                                         StructuralSelection& selection);

    bool request_delete(const StructuralSelection& target, const EditableSystemDraft& draft,
                        const std::vector<DraftTaskAssignment>& assignments);
    const std::optional<SystemDeletionImpact>& pending_delete() const { return pending_delete_; }
    void cancel_delete() { pending_delete_.reset(); }
    SystemExplorerActionResult confirm_delete(EditableSystemDraft& draft,
                                              std::vector<DraftTaskAssignment>& assignments,
                                              StructuralSelection& selection);

  private:
    std::optional<SystemDeletionImpact> pending_delete_;
};

} // namespace cpssim
