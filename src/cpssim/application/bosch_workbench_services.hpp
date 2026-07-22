/*** Shared Bosch-aware services for toolkit-independent workbench owners. ***/
#pragma once

#include "cpssim/application/workbench_application.hpp"

#include <filesystem>

namespace cpssim {

WorkbenchApplicationServices
make_bosch_workbench_services(const std::filesystem::path& reference_root,
                              const std::filesystem::path& shared_library);

} // namespace cpssim
