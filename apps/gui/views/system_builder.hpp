/*** Declare the Goal 2 forms-and-tables system editor. ***/

#pragma once

#include "cpssim/gui/draft_run_plan.hpp"
#include "cpssim/gui/editable_system_draft.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace cpssim::gui {

enum class SystemBuilderAction {
    None,
    ApplyAndRestart,
};

struct SystemBuilderViewState {
    std::size_t profile_resource_page{0};
    std::string status;
    bool status_error{false};
};

SystemBuilderAction draw_system_builder(EditableSystemDraft& draft,
                                        SystemDraftBuildResult& validation,
                                        std::vector<DraftTaskAssignment>& assignments,
                                        bool changes_dirty, std::string_view project_name,
                                        SystemBuilderViewState& state);

} // namespace cpssim::gui
