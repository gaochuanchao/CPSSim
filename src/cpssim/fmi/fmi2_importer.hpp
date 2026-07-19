/***
 * File: src/cpssim/fmi/fmi2_importer.hpp
 * Purpose: Declare a small FMI 2.0 Co-Simulation lifecycle and typed-access
 *          adapter without exposing FMI C types to the simulator core.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: Floating-point seconds and FMI value references exist only at this
 *        external adapter boundary. T16 will derive time from integer ticks.
 ***/

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

/*** Numeric handle assigned to one scalar variable by an FMI model. ***/
using Fmi2ValueReference = std::uint32_t;

/*** Mirrors the six statuses returned by FMI 2.0 runtime functions. ***/
enum class Fmi2Status {
    Ok,
    Warning,
    Discard,
    Error,
    Fatal,
    Pending,
};

/*** Describes the adapter-owned lifecycle of one Co-Simulation component. ***/
enum class Fmi2Lifecycle {
    Loaded,
    Initialized,
    Terminated,
};

/*** Returns one FMI status together with a stable adapter diagnostic. ***/
struct Fmi2CallResult {
    Fmi2Status status;
    std::string message;

    // Reports whether FMI says the requested call completed.
    bool succeeded() const { return status == Fmi2Status::Ok || status == Fmi2Status::Warning; }
};

/***
 * Supplies the prepared platform library and FMI identity needed to create
 * one component. Archive extraction and XML parsing are outside T15.
 ***/
struct Fmi2ModelInfo {
    std::filesystem::path shared_library;
    std::string model_identifier;
    std::string guid;
    std::string resource_uri;
    std::string instance_name;
};

/*** Stores one Real parameter assignment applied during initialization. ***/
struct Fmi2InitialReal {
    Fmi2ValueReference reference;
    double value;
};

/***
 * Owns one dynamically loaded FMI 2.0 Co-Simulation component.
 * Construction throws std::invalid_argument for incomplete model metadata and
 * std::runtime_error when the library or a required symbol cannot be loaded.
 * Runtime FMI failures are returned as Fmi2CallResult values.
 ***/
class Fmi2CoSimulation {
  public:
    // Loads the shared library and its required FMI 2.0 function table.
    Fmi2CoSimulation(const Fmi2ModelInfo& model);

    // Performs best-effort component cleanup and closes the shared library.
    ~Fmi2CoSimulation();

    // Returns the current adapter-owned lifecycle state.
    Fmi2Lifecycle lifecycle() const;

    /***
     * Instantiates and initializes the component in FMI-defined order.
     * A missing stop_time means the experiment has no declared stop time.
     * Failure frees any partial instance and returns the adapter to Loaded.
     ***/
    Fmi2CallResult initialize(double start_time, std::optional<double> stop_time = std::nullopt);

    /***
     * Initializes while applying Real parameter values after entering and
     * before exiting FMI initialization mode. Any failed assignment rolls the
     * partial component back to Loaded.
     ***/
    Fmi2CallResult initialize(double start_time, std::optional<double> stop_time,
                              const std::vector<Fmi2InitialReal>& initial_reals);

    // Writes one FMI Real while the component is Initialized.
    Fmi2CallResult set_real(Fmi2ValueReference reference, double value);

    // Reads one FMI Real into value only when the call succeeds.
    Fmi2CallResult get_real(Fmi2ValueReference reference, double& value);

    // Writes one FMI Integer while the component is Initialized.
    Fmi2CallResult set_integer(Fmi2ValueReference reference, std::int32_t value);

    // Reads one FMI Integer into value only when the call succeeds.
    Fmi2CallResult get_integer(Fmi2ValueReference reference, std::int32_t& value);

    // Writes one FMI Boolean while the component is Initialized.
    Fmi2CallResult set_boolean(Fmi2ValueReference reference, bool value);

    // Reads one FMI Boolean into value only when the call succeeds.
    Fmi2CallResult get_boolean(Fmi2ValueReference reference, bool& value);

    // Writes one FMI String while the component is Initialized.
    Fmi2CallResult set_string(Fmi2ValueReference reference, const std::string& value);

    // Copies one FMU-owned String into value only when the call succeeds.
    Fmi2CallResult get_string(Fmi2ValueReference reference, std::string& value);

    /***
     * Advances an Initialized component by one communication interval.
     * The caller supplies seconds because FMI 2.0 requires floating point.
     ***/
    Fmi2CallResult do_step(double current_communication_point, double communication_step_size);

    /***
     * Terminates and frees an Initialized component. The component is freed
     * even when fmi2Terminate reports failure, so the object becomes terminal.
     ***/
    Fmi2CallResult terminate();

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cpssim
