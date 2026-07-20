/*** Declare Explorer-owned structural navigation and lifecycle rendering. ***/

#pragma once

#include "cpssim/gui/presentation_model.hpp"
#include "cpssim/gui/system_builder_interaction.hpp"

#include <optional>
#include <string>

namespace cpssim::gui {

struct ExperimentExplorerViewState {
    std::optional<StructuralSection> expand_section;
    std::optional<StructuralSelection> scroll_to;
    SystemBuilderFocusTarget focus_request{SystemBuilderFocusTarget::None};
    std::string status;
    bool status_error{false};
};

/*** Draws draft structure and delegates every lifecycle mutation to the controller. ***/
void draw_experiment_explorer(const ExperimentPresentationSnapshot& applied,
                              EditableSystemDraft* draft,
                              std::vector<DraftTaskAssignment>& assignments,
                              StructuralSelection& selection,
                              SystemExplorerInteraction& interaction, bool editing_enabled,
                              ExperimentExplorerViewState& state);

} // namespace cpssim::gui
