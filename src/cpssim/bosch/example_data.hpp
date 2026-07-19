/***
 * File: src/cpssim/bosch/example_data.hpp
 * Purpose: Declare strict loading of the Bosch challenge example CSV inputs.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Example time values are validation input only. Canonical simulator
 *        time remains integer Tick.
 ***/

#pragma once

#include "cpssim/bosch/bosch_fmi2_functional_model.hpp"

#include <filesystem>
#include <vector>

namespace cpssim {

/***
 * Loads one Bosch example_v_* directory in row lockstep. The time, position,
 * feedforward, and velocity columns must all be present, finite, equally long,
 * and sampled at consecutive 100-microsecond integer ticks. Position values
 * are validated but are not inputs of the Bosch v10 FMU.
 ***/
std::vector<BoschTrajectorySample>
load_bosch_example_trajectory(const std::filesystem::path& example_directory);

} // namespace cpssim
