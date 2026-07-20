/***
 * File: src/cpssim/gui/signal_series.hpp
 * Purpose: Declare graphics-independent G06 scalar signal presentation data.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#pragma once

#include "cpssim/functional/functional_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cpssim {

enum class GuiSignalScalarType {
    Real,
    Integer,
    Boolean,
};

/*** Stable identity derived from typed adapter signal identity, never display text. ***/
struct GuiSignalId {
    GuiSignalScalarType scalar_type;
    std::string source_name;

    auto operator<=>(const GuiSignalId&) const = default;
};

/*** Optional adapter-provided presentation metadata for one stable signal. ***/
struct GuiSignalDescriptor {
    GuiSignalId id;
    std::string path;
    std::string display_name;
    std::string unit;
    std::string source;

    bool operator==(const GuiSignalDescriptor&) const = default;
};

using GuiScalarValue = std::variant<double, std::int64_t, bool>;

/*** Retains the exact integer sample tick and original scalar value type. ***/
struct GuiScalarSample {
    Tick tick;
    GuiScalarValue value;

    bool operator==(const GuiScalarSample&) const = default;
};

/*** Owns one descriptor and its complete full-resolution sample sequence. ***/
struct GuiSignalSeries {
    GuiSignalDescriptor descriptor;
    std::vector<GuiScalarSample> samples;

    bool operator==(const GuiSignalSeries&) const = default;
};

struct GuiSignalModel {
    std::vector<GuiSignalSeries> series;

    bool operator==(const GuiSignalModel&) const = default;
};

enum class GuiSignalDiagnosticCode {
    InvalidRegistry,
    InvalidObservationTick,
    InvalidSignal,
    SchemaMismatch,
};

/*** Locates the first malformed registry field, observation, or signal. ***/
struct GuiSignalDiagnostic {
    GuiSignalDiagnosticCode code;
    std::size_t observation_index;
    std::optional<Tick> tick;
    std::optional<GuiSignalId> signal_id;
    std::string message;

    bool operator==(const GuiSignalDiagnostic&) const = default;
};

struct GuiSignalBuildResult {
    std::optional<GuiSignalModel> model;
    std::vector<GuiSignalDiagnostic> diagnostics;

    bool valid() const { return model.has_value() && diagnostics.empty(); }
};

/*** Strictly extracts typed scalar series from detached functional observations. ***/
GuiSignalBuildResult build_signal_model(const std::vector<FunctionalObservation>& observations,
                                        const std::vector<GuiSignalDescriptor>& registry = {});

/*** Converts a typed value to a plotting coordinate without changing stored data. ***/
double gui_scalar_as_double(const GuiScalarValue& value);

/*** Finds one series by stable identity in logarithmic time. ***/
const GuiSignalSeries* find_signal_series(const GuiSignalModel& model, const GuiSignalId& id);

/*** Names the viewport and rendering budget used for one downsampling request. ***/
struct GuiSignalDownsampleRequest {
    Tick begin_tick;
    Tick end_tick;
    std::size_t maximum_points;
};

/***
 * Returns deterministic visible samples, preserving endpoints and per-bucket
 * extrema. The source series is never modified.
 ***/
std::vector<GuiScalarSample> downsample_signal(const GuiSignalSeries& series,
                                               GuiSignalDownsampleRequest request);

/*** Retains validated schema and appends only unseen observation rows. ***/
class GuiSignalCache {
  public:
    GuiSignalCache();
    ~GuiSignalCache();
    GuiSignalCache(GuiSignalCache&&) noexcept;
    GuiSignalCache& operator=(GuiSignalCache&&) noexcept;
    GuiSignalCache(const GuiSignalCache&) = delete;
    GuiSignalCache& operator=(const GuiSignalCache&) = delete;

    const GuiSignalBuildResult& update(const std::vector<FunctionalObservation>& observations,
                                       const std::vector<GuiSignalDescriptor>& registry = {});
    void clear();

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace cpssim
