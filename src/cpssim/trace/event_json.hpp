/***
 * File: src/cpssim/trace/event_json.hpp
 * Purpose: Declare deterministic JSON Lines serialization for canonical
 *          CPSSim events.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The public interface exposes only Event and std::string; JSON library
 *        types remain private to the implementation.
 ***/

#pragma once

#include "cpssim/model/event.hpp"

#include <string>

namespace cpssim {

/***
 * Serializes one event using the canonical field order and appends exactly one
 * newline character. Absent optional references are written as JSON null.
 * Throws std::logic_error if an invalid enum value reaches serialization.
 ***/
std::string serialize_event_json_line(const Event& event);

} // namespace cpssim
