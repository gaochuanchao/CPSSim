/***
 * File: tests/smoke_test.cpp
 * Purpose: Verify that the linked core library exposes the expected project
 *          version through its smallest public interface.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/core/version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

/*** Verifies that the build embeds and returns the configured CPSSim version. ***/
TEST_CASE("the core exposes the project version", "[smoke]") {
    const auto version_matches = cpssim::version() == std::string_view{"0.1.0"};

    REQUIRE(version_matches);
}
