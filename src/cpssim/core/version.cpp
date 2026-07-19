/***
 * File: src/cpssim/core/version.cpp
 * Purpose: Implement access to the build-time CPSSim version string.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: CMake must define CPSSIM_VERSION; compilation stops immediately if
 *        that build contract is missing.
 ***/

#include "cpssim/core/version.hpp"

#ifndef CPSSIM_VERSION
#error "CPSSIM_VERSION must be supplied by the build system"
#endif

namespace cpssim {

/***
 * Returns a non-owning view of the version text supplied by the build system.
 ***/
std::string_view version() { return CPSSIM_VERSION; }

} // namespace cpssim
