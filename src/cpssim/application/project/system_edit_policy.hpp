/*** Define project-level structural edit permissions and Bosch compatibility checks. ***/

#pragma once

#include "cpssim/application/project/project.hpp"
#include "cpssim/gui/editable_system_draft.hpp"

#include <string>
#include <vector>

namespace cpssim {

enum class ProjectSystemEditPolicy { Generic, BoschCompatible, ReadOnlyAdapter };
enum class BoschExperimentStatus { NotBosch, ReferenceBaseline, Modified };

ProjectSystemEditPolicy project_system_edit_policy(const ProjectMetadata& metadata) noexcept;
std::vector<std::string> validate_project_system_edit_policy(const ProjectContext& project,
                                                             const EditableSystemDraft& draft);

} // namespace cpssim
