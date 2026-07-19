/***
 * File: src/cpssim/gui/signal_series.cpp
 * Purpose: Implement strict G06 signal extraction, caching, and downsampling.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 ***/

#include "cpssim/gui/signal_series.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace cpssim {
namespace {

const char* type_name(GuiSignalScalarType type) {
    switch (type) {
    case GuiSignalScalarType::Real:
        return "Real";
    case GuiSignalScalarType::Integer:
        return "Integer";
    case GuiSignalScalarType::Boolean:
        return "Boolean";
    }
    return "Unknown";
}

GuiSignalDescriptor fallback_descriptor(GuiSignalId id) {
    const auto type = std::string{type_name(id.scalar_type)};
    const auto name = id.source_name;
    return {.id = std::move(id),
            .path = "Functional/" + type + "/" + name,
            .display_name = name,
            .unit = "",
            .source = "functional"};
}

GuiSignalBuildResult fail(GuiSignalDiagnosticCode code, std::size_t observation_index,
                          std::optional<Tick> tick, std::optional<GuiSignalId> signal_id,
                          std::string reason) {
    auto location = "signal observation[" + std::to_string(observation_index) + "]";
    if (tick.has_value()) {
        location += " at tick " + std::to_string(*tick);
    }
    if (signal_id.has_value()) {
        location += " " + std::string{type_name(signal_id->scalar_type)} + " signal '" +
                    signal_id->source_name + "'";
    }
    return {.model = std::nullopt,
            .diagnostics = {{.code = code,
                             .observation_index = observation_index,
                             .tick = tick,
                             .signal_id = std::move(signal_id),
                             .message = location + ": " + std::move(reason)}}};
}

GuiSignalBuildResult fail_registry(std::size_t registry_index, GuiSignalId signal_id,
                                   std::string reason) {
    auto location = "signal registry[" + std::to_string(registry_index) + "]";
    if (!signal_id.source_name.empty()) {
        location += " " + std::string{type_name(signal_id.scalar_type)} + " signal '" +
                    signal_id.source_name + "'";
    }
    return {.model = std::nullopt,
            .diagnostics = {{.code = GuiSignalDiagnosticCode::InvalidRegistry,
                             .observation_index = registry_index,
                             .tick = std::nullopt,
                             .signal_id = std::move(signal_id),
                             .message = location + ": " + std::move(reason)}}};
}

struct RowEntry {
    GuiSignalId id;
    GuiScalarValue value;
};

struct RowResult {
    std::vector<RowEntry> entries;
    GuiSignalBuildResult failure;
};

RowResult read_row(const FunctionalObservation& observation, std::size_t index) {
    std::set<std::string> names;
    std::vector<RowEntry> entries;
    entries.reserve(observation.real_signals.size() + observation.integer_signals.size() +
                    observation.boolean_signals.size());

    for (const auto& signal : observation.real_signals) {
        const GuiSignalId id{GuiSignalScalarType::Real, signal.name};
        if (signal.name.empty() || !std::isfinite(signal.value) ||
            !names.insert(signal.name).second) {
            return {.entries = {},
                    .failure =
                        fail(GuiSignalDiagnosticCode::InvalidSignal, index, observation.tick, id,
                             "name must be unique and nonempty and value must be finite")};
        }
        entries.push_back({.id = id, .value = signal.value});
    }
    for (const auto& signal : observation.integer_signals) {
        const GuiSignalId id{GuiSignalScalarType::Integer, signal.name};
        if (signal.name.empty() || !names.insert(signal.name).second) {
            return {.entries = {},
                    .failure = fail(GuiSignalDiagnosticCode::InvalidSignal, index, observation.tick,
                                    id, "name must be unique and nonempty")};
        }
        entries.push_back({.id = id, .value = signal.value});
    }
    for (const auto& signal : observation.boolean_signals) {
        const GuiSignalId id{GuiSignalScalarType::Boolean, signal.name};
        if (signal.name.empty() || !names.insert(signal.name).second) {
            return {.entries = {},
                    .failure = fail(GuiSignalDiagnosticCode::InvalidSignal, index, observation.tick,
                                    id, "name must be unique and nonempty")};
        }
        entries.push_back({.id = id, .value = signal.value});
    }
    std::sort(entries.begin(), entries.end(),
              [](const RowEntry& left, const RowEntry& right) { return left.id < right.id; });
    return {.entries = std::move(entries), .failure = {}};
}

GuiSignalBuildResult validate_registry(const std::vector<GuiSignalDescriptor>& registry) {
    std::set<GuiSignalId> identities;
    for (std::size_t index = 0; index < registry.size(); ++index) {
        const auto& descriptor = registry[index];
        if (descriptor.id.source_name.empty()) {
            return fail_registry(index, descriptor.id, "identity source name must not be empty");
        }
        if (descriptor.path.empty()) {
            return fail_registry(index, descriptor.id, "path must not be empty");
        }
        if (descriptor.display_name.empty()) {
            return fail_registry(index, descriptor.id, "display name must not be empty");
        }
        if (descriptor.source.empty()) {
            return fail_registry(index, descriptor.id, "source must not be empty");
        }
        if (!identities.insert(descriptor.id).second) {
            return fail_registry(index, descriptor.id, "identity is duplicated");
        }
    }
    return {.model = GuiSignalModel{}, .diagnostics = {}};
}

std::vector<GuiSignalDescriptor>
initial_descriptors(const std::vector<RowEntry>& first_row,
                    const std::vector<GuiSignalDescriptor>& registry) {
    if (!registry.empty()) {
        auto descriptors = registry;
        std::sort(descriptors.begin(), descriptors.end(),
                  [](const GuiSignalDescriptor& left, const GuiSignalDescriptor& right) {
                      return left.id < right.id;
                  });
        return descriptors;
    }

    std::vector<GuiSignalDescriptor> descriptors;
    descriptors.reserve(first_row.size());
    for (const auto& entry : first_row) {
        descriptors.push_back(fallback_descriptor(entry.id));
    }
    return descriptors;
}

bool same_observation(const FunctionalObservation& left, const FunctionalObservation& right) {
    if (left.tick != right.tick || left.real_signals.size() != right.real_signals.size() ||
        left.integer_signals.size() != right.integer_signals.size() ||
        left.boolean_signals.size() != right.boolean_signals.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.real_signals.size(); ++index) {
        if (left.real_signals[index].name != right.real_signals[index].name ||
            left.real_signals[index].value != right.real_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.integer_signals.size(); ++index) {
        if (left.integer_signals[index].name != right.integer_signals[index].name ||
            left.integer_signals[index].value != right.integer_signals[index].value) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.boolean_signals.size(); ++index) {
        if (left.boolean_signals[index].name != right.boolean_signals[index].name ||
            left.boolean_signals[index].value != right.boolean_signals[index].value) {
            return false;
        }
    }
    return true;
}

/*** Owns one validated schema and its incrementally appended full-resolution rows. ***/
class SignalAccumulator {
  public:
    explicit SignalAccumulator(std::vector<GuiSignalDescriptor> descriptors) {
        model_.series.reserve(descriptors.size());
        for (auto& descriptor : descriptors) {
            model_.series.push_back({.descriptor = std::move(descriptor), .samples = {}});
        }
    }

    GuiSignalBuildResult append(const FunctionalObservation& observation, std::size_t index) {
        if (observation.tick != static_cast<Tick>(index)) {
            return fail(GuiSignalDiagnosticCode::InvalidObservationTick, index, observation.tick,
                        std::nullopt, "expected consecutive tick " + std::to_string(index));
        }
        const auto row = read_row(observation, index);
        if (!row.failure.diagnostics.empty()) {
            return row.failure;
        }
        const auto shared_size = std::min(row.entries.size(), model_.series.size());
        for (std::size_t signal_index = 0; signal_index < shared_size; ++signal_index) {
            if (row.entries[signal_index].id != model_.series[signal_index].descriptor.id) {
                return fail(GuiSignalDiagnosticCode::SchemaMismatch, index, observation.tick,
                            row.entries[signal_index].id,
                            "identity differs from the initial registry");
            }
        }
        if (row.entries.size() < model_.series.size()) {
            return fail(GuiSignalDiagnosticCode::SchemaMismatch, index, observation.tick,
                        model_.series[row.entries.size()].descriptor.id,
                        "required signal is missing from this observation");
        }
        if (row.entries.size() > model_.series.size()) {
            return fail(GuiSignalDiagnosticCode::SchemaMismatch, index, observation.tick,
                        row.entries[model_.series.size()].id,
                        "signal is not present in the initial registry");
        }
        for (std::size_t signal_index = 0; signal_index < row.entries.size(); ++signal_index) {
            model_.series[signal_index].samples.push_back(
                {.tick = observation.tick, .value = row.entries[signal_index].value});
        }
        return {};
    }

    GuiSignalBuildResult snapshot() const { return {.model = model_, .diagnostics = {}}; }

  private:
    GuiSignalModel model_;
};

GuiSignalBuildResult build_with_accumulator(const std::vector<FunctionalObservation>& observations,
                                            const std::vector<GuiSignalDescriptor>& registry,
                                            std::optional<SignalAccumulator>& accumulator) {
    const auto registry_validation = validate_registry(registry);
    if (!registry_validation.valid()) {
        return registry_validation;
    }

    std::vector<RowEntry> first_entries;
    if (!observations.empty()) {
        const auto first = read_row(observations.front(), 0);
        if (!first.failure.diagnostics.empty()) {
            return first.failure;
        }
        first_entries = first.entries;
    }
    auto descriptors = initial_descriptors(first_entries, registry);
    accumulator.emplace(std::move(descriptors));
    for (std::size_t index = 0; index < observations.size(); ++index) {
        const auto appended = accumulator->append(observations[index], index);
        if (!appended.diagnostics.empty()) {
            accumulator.reset();
            return appended;
        }
    }
    return accumulator->snapshot();
}

} // namespace

GuiSignalBuildResult build_signal_model(const std::vector<FunctionalObservation>& observations,
                                        const std::vector<GuiSignalDescriptor>& registry) {
    std::optional<SignalAccumulator> accumulator;
    return build_with_accumulator(observations, registry, accumulator);
}

double gui_scalar_as_double(const GuiScalarValue& value) {
    return std::visit([](const auto& typed_value) { return static_cast<double>(typed_value); },
                      value);
}

const GuiSignalSeries* find_signal_series(const GuiSignalModel& model, const GuiSignalId& id) {
    const auto found =
        std::lower_bound(model.series.begin(), model.series.end(), id,
                         [](const GuiSignalSeries& series, const GuiSignalId& candidate) {
                             return series.descriptor.id < candidate;
                         });
    return found != model.series.end() && found->descriptor.id == id ? &*found : nullptr;
}

std::vector<GuiScalarSample> downsample_signal(const GuiSignalSeries& series, Tick begin_tick,
                                               Tick end_tick, std::size_t maximum_points) {
    if (end_tick < begin_tick) {
        std::swap(begin_tick, end_tick);
    }
    const auto begin = std::lower_bound(
        series.samples.begin(), series.samples.end(), begin_tick,
        [](const GuiScalarSample& sample, Tick tick) { return sample.tick < tick; });
    const auto end = std::upper_bound(
        begin, series.samples.end(), end_tick,
        [](Tick tick, const GuiScalarSample& sample) { return tick < sample.tick; });
    const auto count = static_cast<std::size_t>(std::distance(begin, end));
    if (count == 0 || maximum_points == 0) {
        return {};
    }
    if (count <= maximum_points) {
        return {begin, end};
    }
    if (maximum_points == 1) {
        return {*begin};
    }
    if (maximum_points < 4) {
        return {*begin, *std::prev(end)};
    }

    std::vector<GuiScalarSample> result;
    result.reserve(maximum_points);
    result.push_back(*begin);
    const auto interior_count = count - 2;
    const auto bucket_count = std::max<std::size_t>((maximum_points - 2) / 2, 1);
    for (std::size_t bucket = 0; bucket < bucket_count; ++bucket) {
        const auto bucket_begin_offset = 1 + (interior_count * bucket) / bucket_count;
        const auto bucket_end_offset = 1 + (interior_count * (bucket + 1)) / bucket_count;
        if (bucket_begin_offset == bucket_end_offset) {
            continue;
        }
        const auto bucket_begin =
            std::next(begin, static_cast<std::ptrdiff_t>(bucket_begin_offset));
        const auto bucket_end = std::next(begin, static_cast<std::ptrdiff_t>(bucket_end_offset));
        const auto minimum = std::min_element(
            bucket_begin, bucket_end,
            [](const GuiScalarSample& left, const GuiScalarSample& right) {
                return gui_scalar_as_double(left.value) < gui_scalar_as_double(right.value);
            });
        const auto maximum = std::max_element(
            bucket_begin, bucket_end,
            [](const GuiScalarSample& left, const GuiScalarSample& right) {
                return gui_scalar_as_double(left.value) < gui_scalar_as_double(right.value);
            });
        if (minimum <= maximum) {
            result.push_back(*minimum);
            if (minimum != maximum) {
                result.push_back(*maximum);
            }
        } else {
            result.push_back(*maximum);
            result.push_back(*minimum);
        }
    }
    result.push_back(*std::prev(end));
    return result;
}

struct GuiSignalCache::State {
    std::vector<GuiSignalDescriptor> registry;
    std::optional<SignalAccumulator> accumulator;
    std::size_t observation_count{0};
    std::optional<FunctionalObservation> boundary_observation;
    GuiSignalBuildResult result;
};

GuiSignalCache::GuiSignalCache() : state_{std::make_unique<State>()} {}
GuiSignalCache::~GuiSignalCache() = default;
GuiSignalCache::GuiSignalCache(GuiSignalCache&&) noexcept = default;
GuiSignalCache& GuiSignalCache::operator=(GuiSignalCache&&) noexcept = default;

void GuiSignalCache::clear() { state_ = std::make_unique<State>(); }

const GuiSignalBuildResult&
GuiSignalCache::update(const std::vector<FunctionalObservation>& observations,
                       const std::vector<GuiSignalDescriptor>& registry) {
    const auto prefix_compatible =
        state_->accumulator.has_value() && state_->registry == registry &&
        observations.size() >= state_->observation_count && state_->observation_count > 0 &&
        same_observation(observations[state_->observation_count - 1],
                         *state_->boundary_observation);
    if (!prefix_compatible || state_->observation_count == 0) {
        auto rebuilt = std::make_unique<State>();
        rebuilt->registry = registry;
        rebuilt->result = build_with_accumulator(observations, registry, rebuilt->accumulator);
        if (rebuilt->result.valid()) {
            rebuilt->observation_count = observations.size();
            if (!observations.empty()) {
                rebuilt->boundary_observation = observations.back();
            }
            state_ = std::move(rebuilt);
        } else {
            state_->result = std::move(rebuilt->result);
        }
        return state_->result;
    }

    if (observations.size() == state_->observation_count) {
        if (!state_->result.valid()) {
            state_->result = state_->accumulator->snapshot();
        }
        return state_->result;
    }

    auto candidate = *state_->accumulator;
    for (auto index = state_->observation_count; index < observations.size(); ++index) {
        const auto appended = candidate.append(observations[index], index);
        if (!appended.diagnostics.empty()) {
            state_->result = appended;
            return state_->result;
        }
    }
    state_->accumulator = std::move(candidate);
    state_->observation_count = observations.size();
    state_->boundary_observation = observations.back();
    state_->result = state_->accumulator->snapshot();
    return state_->result;
}

} // namespace cpssim
