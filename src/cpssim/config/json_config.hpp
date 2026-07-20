/***
 * File: src/cpssim/config/json_config.hpp
 * Purpose: Declare the JSON text and file boundaries that create validated
 *          ExperimentConfig values.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: JSON library types remain private to the implementation so the model
 *        interface stays portable and serialization-independent.
 ***/

#pragma once

#include "cpssim/model/experiment_config.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace cpssim {

/***
 * Parses one schema-version-1 JSON document and returns a validated
 * ExperimentConfig.
 * Throws std::invalid_argument for malformed JSON or any schema/value error.
 ***/
ExperimentConfig parse_experiment_config(std::string_view json_text);

// Serializes a validated configuration in the current canonical schema.
std::string serialize_experiment_config_json(const ExperimentConfig& config);

/***
 * Reads an experiment JSON file and delegates parsing and validation to
 * parse_experiment_config.
 * Throws std::runtime_error for file access failures and
 * std::invalid_argument for invalid document content.
 ***/
ExperimentConfig load_experiment_config(const std::filesystem::path& path);

// Serializes before opening the destination for writing.
void save_experiment_config(const std::filesystem::path& path, const ExperimentConfig& config);

} // namespace cpssim
