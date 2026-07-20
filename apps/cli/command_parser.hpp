/***
 * File: apps/cli/command_parser.hpp
 * Purpose: Declare terminal-line tokenization for the interactive CPSSim CLI.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Direct argv execution bypasses this tokenizer and uses the same
 *        command registry with already separated arguments.
 ***/

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cpssim {

/***
 * Splits one command line on whitespace while supporting single quotes,
 * double quotes, and backslash escaping. Throws std::invalid_argument for an
 * unfinished quote or escape.
 ***/
std::vector<std::string> parse_cli_command_line(std::string_view line);

} // namespace cpssim
