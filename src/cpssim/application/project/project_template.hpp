/*** Minimal built-in generic project template used to enter the System Builder. ***/

#pragma once

#include "cpssim/application/project/project.hpp"

namespace cpssim {

ProjectCreationRequest make_generic_project_template(std::filesystem::path parent_directory,
                                                     std::string name);

} // namespace cpssim
