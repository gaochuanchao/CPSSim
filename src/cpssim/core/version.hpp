/***
 * File: src/cpssim/core/version.hpp
 * Purpose: Declare access to the CPSSim version embedded by the build system.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: This small interface lets applications report the library version
 *        without depending directly on CMake definitions.
 ***/

#pragma once

#include <string_view>

namespace cpssim {

/***
 * Returns the CPSSim project version compiled into the core library.
 * The returned string view refers to static build-time text and requires no
 * caller-managed storage.
 ***/
std::string_view version();

} // namespace cpssim
