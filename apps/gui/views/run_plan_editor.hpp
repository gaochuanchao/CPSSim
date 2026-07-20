/*** Declare the right-side run-configuration editor and actions. ***/

#pragma once

#include "cpssim/gui/editable_system_draft.hpp"
#include "cpssim/gui/simulation_session.hpp"

namespace cpssim::gui {

enum class RunConfigurationAction {
    None,
    ValidateChanges,
    ApplyAndRestart,
};

/*** Draws run-plan fields only; structural application is deferred to the caller. ***/
RunConfigurationAction draw_run_configuration(GuiSimulationSession& session,
                                              const SimulationSnapshot& snapshot,
                                              const EditableSystemDraft* system_draft,
                                              std::vector<DraftTaskAssignment>& system_assignments,
                                              bool system_changes_dirty);

} // namespace cpssim::gui
