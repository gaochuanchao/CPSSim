/***
 * File: tests/fmi/fmi2_importer_test.cpp
 * Purpose: Verify FMI 2.0 loading, lifecycle, typed access, stepping, runtime
 *          error propagation, and cleanup against the supplied Bosch source.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: CTest supplies the build-tree Linux shared-library path through an
 *        environment variable; the tracked Win64 FMU is not modified.
 ***/

#include "cpssim/fmi/fmi2_importer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

/*** Returns the CMake-built Bosch Linux library required by these tests. ***/
std::filesystem::path bosch_library_path() {
    const char* path = std::getenv("CPSSIM_T15_BOSCH_FMU_LIBRARY");
    if (path == nullptr || std::string{path}.empty()) {
        throw std::runtime_error{"CPSSIM_T15_BOSCH_FMU_LIBRARY is not set"};
    }
    return path;
}

/*** Creates the identity declared by the tracked Bosch modelDescription.xml. ***/
cpssim::Fmi2ModelInfo bosch_model_info() {
    return {.shared_library = bosch_library_path(),
            .model_identifier = "LateralMotionControl",
            .guid = "{ec101913-52ec-40d8-afe6-5fbb52430f74}",
            .resource_uri = "",
            .instance_name = "cpssim_t15_test"};
}

} // namespace

/*** Verifies load, initialization, every typed API, one step, and cleanup. ***/
TEST_CASE("FMI2 importer drives the Bosch Co-Simulation lifecycle", "[fmi2][bosch]") {
    cpssim::Fmi2CoSimulation model{bosch_model_info()};
    REQUIRE((model.lifecycle() == cpssim::Fmi2Lifecycle::Loaded));

    const auto initialized = model.initialize(0.0, 1.0);
    REQUIRE(initialized.succeeded());
    REQUIRE((model.lifecycle() == cpssim::Fmi2Lifecycle::Initialized));

    REQUIRE(model.set_real(118, 12.5).succeeded());
    double velocity = 0.0;
    REQUIRE(model.get_real(118, velocity).succeeded());
    REQUIRE((velocity == 12.5));

    REQUIRE(model.set_boolean(100, true).succeeded());
    bool sensor_trigger = false;
    REQUIRE(model.get_boolean(100, sensor_trigger).succeeded());
    REQUIRE(sensor_trigger);

    std::int32_t step = -1;
    REQUIRE(model.get_integer(1024, step).succeeded());
    REQUIRE((step == 0));

    REQUIRE(model.do_step(0.0, 0.0001).succeeded());
    double current_time = 0.0;
    REQUIRE(model.get_real(1023, current_time).succeeded());
    REQUIRE((std::abs(current_time - 0.0001) < 1e-12));
    REQUIRE(model.get_integer(1024, step).succeeded());
    REQUIRE((step == 1));

    REQUIRE(model.terminate().succeeded());
    REQUIRE((model.lifecycle() == cpssim::Fmi2Lifecycle::Terminated));
}

/*** Verifies Real parameters are applied before exit initialization mode. ***/
TEST_CASE("FMI2 importer applies initialization Real values", "[fmi2][initialize]") {
    cpssim::Fmi2CoSimulation model{bosch_model_info()};
    const auto initialized = model.initialize(0.0, 1.0, {{.reference = 20, .value = 0.1}});
    double lateral_error = 0.0;

    const bool value_matches = initialized.succeeded() &&
                               model.get_real(1004, lateral_error).succeeded() &&
                               lateral_error == 0.1;
    REQUIRE(value_matches);
    REQUIRE(model.terminate().succeeded());
}

/*** Verifies invalid wrapper state is reported without calling the FMU. ***/
TEST_CASE("FMI2 importer rejects calls outside Initialized", "[fmi2][lifecycle]") {
    cpssim::Fmi2CoSimulation model{bosch_model_info()};
    double value = 7.0;

    const auto read = model.get_real(1023, value);
    const auto step = model.do_step(0.0, 0.0001);
    const auto terminate = model.terminate();

    REQUIRE_FALSE(read.succeeded());
    REQUIRE_FALSE(step.succeeded());
    REQUIRE_FALSE(terminate.succeeded());
    REQUIRE((value == 7.0));
    REQUIRE((model.lifecycle() == cpssim::Fmi2Lifecycle::Loaded));
}

/*** Verifies FMI errors retain status and leave output values unchanged. ***/
TEST_CASE("FMI2 importer propagates typed access errors", "[fmi2][error]") {
    cpssim::Fmi2CoSimulation model{bosch_model_info()};
    REQUIRE(model.initialize(0.0).succeeded());

    double real_value = 9.0;
    std::int32_t integer_value = 9;
    bool boolean_value = true;
    std::string string_value{"unchanged"};

    const auto real_result = model.get_real(999999, real_value);
    const auto integer_result = model.set_integer(999999, integer_value);
    const auto boolean_result = model.get_boolean(999999, boolean_value);
    const auto string_result = model.get_string(999999, string_value);
    const auto string_write_result = model.set_string(999999, "unsupported");

    REQUIRE((real_result.status == cpssim::Fmi2Status::Error));
    REQUIRE((integer_result.status == cpssim::Fmi2Status::Error));
    REQUIRE((boolean_result.status == cpssim::Fmi2Status::Error));
    REQUIRE((string_result.status == cpssim::Fmi2Status::Error));
    REQUIRE((string_write_result.status == cpssim::Fmi2Status::Error));
    REQUIRE((real_value == 9.0));
    REQUIRE(boolean_value);
    REQUIRE((string_value == "unchanged"));
    REQUIRE(model.terminate().succeeded());
}

/*** Verifies failed instantiation is cleaned up and may be retried safely. ***/
TEST_CASE("FMI2 importer reports an incorrect GUID", "[fmi2][error]") {
    auto info = bosch_model_info();
    info.guid = "{incorrect-guid}";
    cpssim::Fmi2CoSimulation model{info};

    const auto result = model.initialize(0.0);
    REQUIRE((result.status == cpssim::Fmi2Status::Error));
    REQUIRE((model.lifecycle() == cpssim::Fmi2Lifecycle::Loaded));
}

/*** Verifies unusable model metadata fails before a runtime component exists. ***/
TEST_CASE("FMI2 importer rejects missing libraries and identifiers", "[fmi2][load]") {
    auto missing_library = bosch_model_info();
    missing_library.shared_library = "missing-fmu-library.so";
    REQUIRE_THROWS_AS(cpssim::Fmi2CoSimulation{missing_library}, std::runtime_error);

    auto missing_identifier = bosch_model_info();
    missing_identifier.model_identifier.clear();
    REQUIRE_THROWS_AS(cpssim::Fmi2CoSimulation{missing_identifier}, std::invalid_argument);
}

/*** Verifies destruction frees an initialized component without explicit terminate. ***/
TEST_CASE("FMI2 importer destructor owns component cleanup", "[fmi2][lifecycle]") {
    {
        cpssim::Fmi2CoSimulation first{bosch_model_info()};
        REQUIRE(first.initialize(0.0).succeeded());
    }

    cpssim::Fmi2CoSimulation second{bosch_model_info()};
    REQUIRE(second.initialize(0.0).succeeded());
    REQUIRE(second.terminate().succeeded());
}
