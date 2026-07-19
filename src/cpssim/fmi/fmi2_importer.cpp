/***
 * File: src/cpssim/fmi/fmi2_importer.cpp
 * Purpose: Load the required FMI 2.0 Co-Simulation C functions, coordinate
 *          component lifecycle, and translate C statuses into C++ results.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The minimal C ABI declarations are private to this translation unit.
 *        No FMI type enters cpssim_core or the public adapter interface.
 ***/

#include "cpssim/fmi/fmi2_importer.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace cpssim {
namespace {

using FmiComponent = void*;
using FmiComponentEnvironment = void*;
using FmiReal = double;
using FmiInteger = int;
using FmiBoolean = int;
using FmiString = const char*;
using FmiValueReference = unsigned int;

enum FmiStatusC {
    FmiOk,
    FmiWarning,
    FmiDiscard,
    FmiError,
    FmiFatal,
    FmiPending,
};

enum FmiTypeC {
    FmiModelExchange,
    FmiCoSimulation,
};

using FmiLogger = void (*)(FmiComponentEnvironment, FmiString, FmiStatusC, FmiString, FmiString,
                           ...);
using FmiAllocateMemory = void* (*)(std::size_t, std::size_t);
using FmiFreeMemory = void (*)(void*);
using FmiStepFinished = void (*)(FmiComponentEnvironment, FmiStatusC);

/*** Matches the callback record defined by the FMI 2.0 C ABI. ***/
struct FmiCallbackFunctions {
    FmiLogger logger;
    FmiAllocateMemory allocate_memory;
    FmiFreeMemory free_memory;
    FmiStepFinished step_finished;
    FmiComponentEnvironment component_environment;
};

using InstantiateFunction = FmiComponent(FmiString, FmiTypeC, FmiString, FmiString,
                                         const FmiCallbackFunctions*, FmiBoolean, FmiBoolean);
using FreeInstanceFunction = void(FmiComponent);
using SetupExperimentFunction = FmiStatusC(FmiComponent, FmiBoolean, FmiReal, FmiReal, FmiBoolean,
                                           FmiReal);
using LifecycleFunction = FmiStatusC(FmiComponent);
using GetRealFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t, FmiReal*);
using SetRealFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                   const FmiReal*);
using GetIntegerFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                      FmiInteger*);
using SetIntegerFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                      const FmiInteger*);
using GetBooleanFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                      FmiBoolean*);
using SetBooleanFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                      const FmiBoolean*);
using GetStringFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                     FmiString*);
using SetStringFunction = FmiStatusC(FmiComponent, const FmiValueReference*, std::size_t,
                                     const FmiString*);
using DoStepFunction = FmiStatusC(FmiComponent, FmiReal, FmiReal, FmiBoolean);

static_assert(sizeof(FmiValueReference) == sizeof(Fmi2ValueReference));
static_assert(sizeof(FmiInteger) == sizeof(std::int32_t));

/*** Supplies the non-null logger callback required by the Bosch FMU. ***/
void ignore_fmi_log(FmiComponentEnvironment, FmiString, FmiStatusC, FmiString, FmiString, ...) {}

// Allocates zero-initialized memory using the FMI callback contract.
void* allocate_fmi_memory(std::size_t count, std::size_t size) { return std::calloc(count, size); }

// Releases memory previously returned by allocate_fmi_memory.
void free_fmi_memory(void* memory) { std::free(memory); }

// Accepts asynchronous completion notifications; T15 uses synchronous doStep.
void ignore_step_finished(FmiComponentEnvironment, FmiStatusC) {}

/*** Converts the private C enumeration without relying on numeric casts. ***/
Fmi2Status public_status(FmiStatusC status) {
    switch (status) {
    case FmiOk:
        return Fmi2Status::Ok;
    case FmiWarning:
        return Fmi2Status::Warning;
    case FmiDiscard:
        return Fmi2Status::Discard;
    case FmiError:
        return Fmi2Status::Error;
    case FmiFatal:
        return Fmi2Status::Fatal;
    case FmiPending:
        return Fmi2Status::Pending;
    }
    return Fmi2Status::Fatal;
}

/*** Returns a stable human-readable spelling for diagnostics and tests. ***/
std::string status_name(Fmi2Status status) {
    switch (status) {
    case Fmi2Status::Ok:
        return "OK";
    case Fmi2Status::Warning:
        return "Warning";
    case Fmi2Status::Discard:
        return "Discard";
    case Fmi2Status::Error:
        return "Error";
    case Fmi2Status::Fatal:
        return "Fatal";
    case Fmi2Status::Pending:
        return "Pending";
    }
    return "unknown";
}

/*** Converts one FMI return value into the public result representation. ***/
Fmi2CallResult call_result(std::string call, FmiStatusC status) {
    const auto converted = public_status(status);
    return {.status = converted,
            .message = std::move(call) + " returned FMI " + status_name(converted)};
}

// Creates an adapter-side failure that did not invoke an FMI function.
Fmi2CallResult adapter_error(std::string message) {
    return {.status = Fmi2Status::Error, .message = std::move(message)};
}

/*** Checks model metadata that is required before opening a library. ***/
void validate_model_info(const Fmi2ModelInfo& model) {
    if (model.shared_library.empty()) {
        throw std::invalid_argument{"FMI shared-library path must not be empty"};
    }
    if (model.model_identifier.empty()) {
        throw std::invalid_argument{"FMI model identifier must not be empty"};
    }
    if (model.guid.empty()) {
        throw std::invalid_argument{"FMI GUID must not be empty"};
    }
    if (model.instance_name.empty()) {
        throw std::invalid_argument{"FMI instance name must not be empty"};
    }
}

/*** Owns one operating-system shared-library handle. ***/
class DynamicLibrary {
  public:
    /*** Opens one library or throws with the platform loader diagnostic. ***/
    DynamicLibrary(const std::filesystem::path& path) : path_{path} {
#if defined(_WIN32)
        handle_ = LoadLibraryW(path.wstring().c_str());
        if (handle_ == nullptr) {
            throw std::runtime_error{"cannot load FMI shared library: " + path.string()};
        }
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_ == nullptr) {
            const char* error = dlerror();
            throw std::runtime_error{"cannot load FMI shared library " + path.string() + ": " +
                                     (error == nullptr ? "unknown loader error" : error)};
        }
#endif
    }

    // Closes the library after every resolved function is no longer used.
    ~DynamicLibrary() {
#if defined(_WIN32)
        if (handle_ != nullptr) {
            FreeLibrary(handle_);
        }
#else
        if (handle_ != nullptr) {
            dlclose(handle_);
        }
#endif
    }

    /*** Finds one required exported symbol or throws with its complete name. ***/
    void* required_symbol(const std::string& name) const {
#if defined(_WIN32)
        const auto symbol = GetProcAddress(handle_, name.c_str());
        if (symbol == nullptr) {
            throw std::runtime_error{"FMI library is missing required symbol " + name + ": " +
                                     path_.string()};
        }
        return reinterpret_cast<void*>(symbol);
#else
        dlerror();
        void* symbol = dlsym(handle_, name.c_str());
        const char* error = dlerror();
        if (error != nullptr) {
            throw std::runtime_error{"FMI library is missing required symbol " + name + ": " +
                                     error};
        }
        return symbol;
#endif
    }

  private:
    std::filesystem::path path_;
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};

/*** Resolves one typed FMI function using the model-identifier prefix. ***/
template <typename Function>
Function* load_function(const DynamicLibrary& library, const std::string& prefix,
                        const std::string& function_name) {
    return reinterpret_cast<Function*>(library.required_symbol(prefix + "_fmi2" + function_name));
}

} // namespace

/*** Holds every native handle and mutable lifecycle value behind the C++ API. ***/
class Fmi2CoSimulation::Impl {
  public:
    /*** Loads the complete function table before a component may be created. ***/
    Impl(const Fmi2ModelInfo& model)
        : model_{model}, library_{model.shared_library},
          instantiate_{
              load_function<InstantiateFunction>(library_, model_.model_identifier, "Instantiate")},
          free_instance_{load_function<FreeInstanceFunction>(library_, model_.model_identifier,
                                                             "FreeInstance")},
          setup_experiment_{load_function<SetupExperimentFunction>(
              library_, model_.model_identifier, "SetupExperiment")},
          enter_initialization_{load_function<LifecycleFunction>(library_, model_.model_identifier,
                                                                 "EnterInitializationMode")},
          exit_initialization_{load_function<LifecycleFunction>(library_, model_.model_identifier,
                                                                "ExitInitializationMode")},
          terminate_{
              load_function<LifecycleFunction>(library_, model_.model_identifier, "Terminate")},
          get_real_{load_function<GetRealFunction>(library_, model_.model_identifier, "GetReal")},
          set_real_{load_function<SetRealFunction>(library_, model_.model_identifier, "SetReal")},
          get_integer_{
              load_function<GetIntegerFunction>(library_, model_.model_identifier, "GetInteger")},
          set_integer_{
              load_function<SetIntegerFunction>(library_, model_.model_identifier, "SetInteger")},
          get_boolean_{
              load_function<GetBooleanFunction>(library_, model_.model_identifier, "GetBoolean")},
          set_boolean_{
              load_function<SetBooleanFunction>(library_, model_.model_identifier, "SetBoolean")},
          get_string_{
              load_function<GetStringFunction>(library_, model_.model_identifier, "GetString")},
          set_string_{
              load_function<SetStringFunction>(library_, model_.model_identifier, "SetString")},
          do_step_{load_function<DoStepFunction>(library_, model_.model_identifier, "DoStep")} {}

    // Ensures component storage is released before DynamicLibrary is destroyed.
    ~Impl() {
        if (component_ != nullptr) {
            if (lifecycle_ == Fmi2Lifecycle::Initialized) {
                terminate_(component_);
            }
            free_instance_(component_);
        }
    }

    // Returns the current lifecycle value owned by this implementation.
    Fmi2Lifecycle lifecycle() const { return lifecycle_; }

    /*** Performs the four required initialization calls with rollback on failure. ***/
    Fmi2CallResult initialize(double start_time, std::optional<double> stop_time,
                              const std::vector<Fmi2InitialReal>& initial_reals) {
        if (lifecycle_ != Fmi2Lifecycle::Loaded) {
            return adapter_error("initialize requires the Loaded lifecycle");
        }
        if (!std::isfinite(start_time) ||
            (stop_time.has_value() && (!std::isfinite(*stop_time) || *stop_time < start_time))) {
            return adapter_error("initialize requires finite time and stop_time >= start_time");
        }

        component_ =
            instantiate_(model_.instance_name.c_str(), FmiCoSimulation, model_.guid.c_str(),
                         model_.resource_uri.c_str(), &callbacks_, 0, 0);
        if (component_ == nullptr) {
            return adapter_error("fmi2Instantiate returned a null component");
        }

        FmiStatusC completed_status = FmiOk;
        auto status =
            setup_experiment_(component_, 0, 0.0, start_time, stop_time.has_value() ? 1 : 0,
                              stop_time.has_value() ? *stop_time : 0.0);
        if (!call_succeeded(status)) {
            return rollback_initialization("fmi2SetupExperiment", status);
        }
        if (status == FmiWarning) {
            completed_status = FmiWarning;
        }

        status = enter_initialization_(component_);
        if (!call_succeeded(status)) {
            return rollback_initialization("fmi2EnterInitializationMode", status);
        }
        if (status == FmiWarning) {
            completed_status = FmiWarning;
        }

        for (const auto& initial : initial_reals) {
            const FmiValueReference reference = initial.reference;
            const FmiReal value = initial.value;
            status = set_real_(component_, &reference, 1, &value);
            if (!call_succeeded(status)) {
                return rollback_initialization("fmi2SetReal(initial)", status);
            }
            if (status == FmiWarning) {
                completed_status = FmiWarning;
            }
        }

        status = exit_initialization_(component_);
        if (!call_succeeded(status)) {
            return rollback_initialization("fmi2ExitInitializationMode", status);
        }
        if (status == FmiWarning) {
            completed_status = FmiWarning;
        }

        lifecycle_ = Fmi2Lifecycle::Initialized;
        return call_result("initialize", completed_status);
    }

    // Writes one FMI Real after validating lifecycle.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) FMI convention is reference, then value.
    Fmi2CallResult set_real(Fmi2ValueReference reference, double value) {
        auto state = require_initialized("fmi2SetReal");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        return call_result("fmi2SetReal", set_real_(component_, &fmi_reference, 1, &value));
    }

    // Reads one FMI Real and commits the output only after success.
    Fmi2CallResult get_real(Fmi2ValueReference reference, double& value) {
        auto state = require_initialized("fmi2GetReal");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        FmiReal result = 0.0;
        const auto status = get_real_(component_, &fmi_reference, 1, &result);
        auto call = call_result("fmi2GetReal", status);
        if (call.succeeded()) {
            value = result;
        }
        return call;
    }

    // Writes one FMI Integer after validating lifecycle.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) FMI convention is reference, then value.
    Fmi2CallResult set_integer(Fmi2ValueReference reference, std::int32_t value) {
        auto state = require_initialized("fmi2SetInteger");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        const FmiInteger fmi_value = value;
        return call_result("fmi2SetInteger",
                           set_integer_(component_, &fmi_reference, 1, &fmi_value));
    }

    // Reads one FMI Integer and commits the output only after success.
    Fmi2CallResult get_integer(Fmi2ValueReference reference, std::int32_t& value) {
        auto state = require_initialized("fmi2GetInteger");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        FmiInteger result = 0;
        const auto status = get_integer_(component_, &fmi_reference, 1, &result);
        auto call = call_result("fmi2GetInteger", status);
        if (call.succeeded()) {
            value = result;
        }
        return call;
    }

    // Writes one FMI Boolean after validating lifecycle.
    Fmi2CallResult set_boolean(Fmi2ValueReference reference, bool value) {
        auto state = require_initialized("fmi2SetBoolean");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        const FmiBoolean fmi_value = value ? 1 : 0;
        return call_result("fmi2SetBoolean",
                           set_boolean_(component_, &fmi_reference, 1, &fmi_value));
    }

    // Reads one FMI Boolean and commits the output only after success.
    Fmi2CallResult get_boolean(Fmi2ValueReference reference, bool& value) {
        auto state = require_initialized("fmi2GetBoolean");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        FmiBoolean result = 0;
        const auto status = get_boolean_(component_, &fmi_reference, 1, &result);
        auto call = call_result("fmi2GetBoolean", status);
        if (call.succeeded()) {
            value = result != 0;
        }
        return call;
    }

    // Writes one FMI String after validating lifecycle.
    Fmi2CallResult set_string(Fmi2ValueReference reference, const std::string& value) {
        auto state = require_initialized("fmi2SetString");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        const FmiString fmi_value = value.c_str();
        return call_result("fmi2SetString", set_string_(component_, &fmi_reference, 1, &fmi_value));
    }

    // Reads and copies one FMU-owned String only after a successful call.
    Fmi2CallResult get_string(Fmi2ValueReference reference, std::string& value) {
        auto state = require_initialized("fmi2GetString");
        if (!state.succeeded()) {
            return state;
        }
        const FmiValueReference fmi_reference = reference;
        FmiString result = nullptr;
        const auto status = get_string_(component_, &fmi_reference, 1, &result);
        auto call = call_result("fmi2GetString", status);
        if (call.succeeded()) {
            if (result == nullptr) {
                return adapter_error("fmi2GetString succeeded with a null String");
            }
            value = result;
        }
        return call;
    }

    /*** Validates seconds and forwards one synchronous Co-Simulation step. ***/
    Fmi2CallResult do_step(double current_communication_point, double communication_step_size) {
        auto state = require_initialized("fmi2DoStep");
        if (!state.succeeded()) {
            return state;
        }
        if (!std::isfinite(current_communication_point) ||
            !std::isfinite(communication_step_size) || current_communication_point < 0.0 ||
            communication_step_size < 0.0) {
            return adapter_error("fmi2DoStep requires finite nonnegative seconds");
        }
        return call_result("fmi2DoStep", do_step_(component_, current_communication_point,
                                                  communication_step_size, 1));
    }

    /*** Calls terminate, frees the instance regardless of status, and becomes terminal. ***/
    Fmi2CallResult terminate() {
        auto state = require_initialized("fmi2Terminate");
        if (!state.succeeded()) {
            return state;
        }
        const auto status = terminate_(component_);
        free_instance_(component_);
        component_ = nullptr;
        lifecycle_ = Fmi2Lifecycle::Terminated;
        return call_result("fmi2Terminate", status);
    }

  private:
    // FMI OK and Warning both mean that the synchronous call completed.
    static bool call_succeeded(FmiStatusC status) {
        return status == FmiOk || status == FmiWarning;
    }

    // Rejects runtime calls until initialization has completed.
    Fmi2CallResult require_initialized(const std::string& call) const {
        if (lifecycle_ != Fmi2Lifecycle::Initialized) {
            return adapter_error(call + " requires the Initialized lifecycle");
        }
        return {.status = Fmi2Status::Ok, .message = call + " may proceed"};
    }

    /*** Frees a partially initialized component and preserves the FMI failure. ***/
    Fmi2CallResult rollback_initialization(const std::string& call, FmiStatusC status) {
        free_instance_(component_);
        component_ = nullptr;
        return call_result(call, status);
    }

    Fmi2ModelInfo model_;
    DynamicLibrary library_;
    InstantiateFunction* instantiate_;
    FreeInstanceFunction* free_instance_;
    SetupExperimentFunction* setup_experiment_;
    LifecycleFunction* enter_initialization_;
    LifecycleFunction* exit_initialization_;
    LifecycleFunction* terminate_;
    GetRealFunction* get_real_;
    SetRealFunction* set_real_;
    GetIntegerFunction* get_integer_;
    SetIntegerFunction* set_integer_;
    GetBooleanFunction* get_boolean_;
    SetBooleanFunction* set_boolean_;
    GetStringFunction* get_string_;
    SetStringFunction* set_string_;
    DoStepFunction* do_step_;
    FmiCallbackFunctions callbacks_{.logger = ignore_fmi_log,
                                    .allocate_memory = allocate_fmi_memory,
                                    .free_memory = free_fmi_memory,
                                    .step_finished = ignore_step_finished,
                                    .component_environment = nullptr};
    FmiComponent component_ = nullptr;
    Fmi2Lifecycle lifecycle_ = Fmi2Lifecycle::Loaded;
};

// Validates metadata before allocating the hidden implementation.
Fmi2CoSimulation::Fmi2CoSimulation(const Fmi2ModelInfo& model) {
    validate_model_info(model);
    impl_ = std::make_unique<Impl>(model);
}

// Delegates cleanup to Impl so its component is freed before its library closes.
Fmi2CoSimulation::~Fmi2CoSimulation() = default;

// Exposes the lifecycle without exposing mutable implementation state.
Fmi2Lifecycle Fmi2CoSimulation::lifecycle() const { return impl_->lifecycle(); }

// Delegates the coordinated initialization sequence to Impl.
Fmi2CallResult Fmi2CoSimulation::initialize(double start_time, std::optional<double> stop_time) {
    return impl_->initialize(start_time, stop_time, {});
}

/*** Delegates initialization-time Real assignments inside FMI init mode. ***/
Fmi2CallResult Fmi2CoSimulation::initialize(double start_time, std::optional<double> stop_time,
                                            const std::vector<Fmi2InitialReal>& initial_reals) {
    return impl_->initialize(start_time, stop_time, initial_reals);
}

// Delegates one typed Real write.
Fmi2CallResult Fmi2CoSimulation::set_real(Fmi2ValueReference reference, double value) {
    return impl_->set_real(reference, value);
}

// Delegates one typed Real read.
Fmi2CallResult Fmi2CoSimulation::get_real(Fmi2ValueReference reference, double& value) {
    return impl_->get_real(reference, value);
}

// Delegates one typed Integer write.
Fmi2CallResult Fmi2CoSimulation::set_integer(Fmi2ValueReference reference, std::int32_t value) {
    return impl_->set_integer(reference, value);
}

// Delegates one typed Integer read.
Fmi2CallResult Fmi2CoSimulation::get_integer(Fmi2ValueReference reference, std::int32_t& value) {
    return impl_->get_integer(reference, value);
}

// Delegates one typed Boolean write.
Fmi2CallResult Fmi2CoSimulation::set_boolean(Fmi2ValueReference reference, bool value) {
    return impl_->set_boolean(reference, value);
}

// Delegates one typed Boolean read.
Fmi2CallResult Fmi2CoSimulation::get_boolean(Fmi2ValueReference reference, bool& value) {
    return impl_->get_boolean(reference, value);
}

// Delegates one typed String write.
Fmi2CallResult Fmi2CoSimulation::set_string(Fmi2ValueReference reference,
                                            const std::string& value) {
    return impl_->set_string(reference, value);
}

// Delegates one typed String read and copy.
Fmi2CallResult Fmi2CoSimulation::get_string(Fmi2ValueReference reference, std::string& value) {
    return impl_->get_string(reference, value);
}

// Delegates one validated communication step.
Fmi2CallResult Fmi2CoSimulation::do_step(double current_communication_point,
                                         double communication_step_size) {
    return impl_->do_step(current_communication_point, communication_step_size);
}

// Delegates terminal cleanup and status propagation.
Fmi2CallResult Fmi2CoSimulation::terminate() { return impl_->terminate(); }

} // namespace cpssim
