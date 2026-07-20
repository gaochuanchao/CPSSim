/*** Declare the selection-driven structural property editor. ***/

#pragma once

#include "cpssim/gui/selection_model.hpp"
#include "cpssim/gui/system_builder_interaction.hpp"

#include <string>
#include <string_view>

namespace cpssim::gui {

struct SystemBuilderViewState {
    SystemBuilderFocusTarget focus_request{SystemBuilderFocusTarget::None};
    std::string status;
    bool status_error{false};
};

/*** Draws only the editor mapped from the current structural selection. ***/
void draw_system_builder(EditableSystemDraft& draft, const SystemDraftBuildResult& validation,
                         std::vector<DraftTaskAssignment>& assignments,
                         StructuralSelection& selection, bool editing_enabled,
                         std::string_view project_name, SystemBuilderViewState& state);

} // namespace cpssim::gui
