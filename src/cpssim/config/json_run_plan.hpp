/***
 * File: src/cpssim/config/json_run_plan.hpp
 * Purpose: Declare strict versioned JSON persistence for validated run plans.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: JSON library types remain private to the implementation. Every input
 *        error reports a root-based JSON location.
 ***/

#pragma once

#include "cpssim/model/run_plan.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace cpssim {

// Serializes one plan with a canonical signature of its expected experiment.
std::string serialize_run_plan_json(const ExperimentConfig& config, const RunPlan& plan);

/***
 * Parses, associates, and validates one run-plan document for the expected
 * experiment. Throws std::invalid_argument with a JSON path on any content
 * error.
 ***/
RunPlan parse_run_plan_json(std::string_view json_text, const ExperimentConfig& config);

// Reads a file and delegates strict parsing to parse_run_plan_json.
RunPlan load_run_plan(const std::filesystem::path& path, const ExperimentConfig& config);

// Validates and serializes before opening the destination for writing.
void save_run_plan(const std::filesystem::path& path, const ExperimentConfig& config,
                   const RunPlan& plan);

} // namespace cpssim
