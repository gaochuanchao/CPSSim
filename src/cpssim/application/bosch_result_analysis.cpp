/*** Derive Bosch plot overlays from typed signal identities and canonical events. ***/

#include "cpssim/application/bosch_result_analysis.hpp"

namespace cpssim {
namespace {

void add_crossings(const GuiSignalSeries& series, double threshold,
                   std::vector<BoschThresholdCrossing>& crossings) {
    for (std::size_t index = 1; index < series.samples.size(); ++index) {
        const auto previous = gui_scalar_as_double(series.samples[index - 1].value);
        const auto current = gui_scalar_as_double(series.samples[index].value);
        const auto previous_side = previous - threshold;
        const auto current_side = current - threshold;
        if ((previous_side < 0.0 && current_side >= 0.0) ||
            (previous_side > 0.0 && current_side <= 0.0)) {
            crossings.push_back({.tick = series.samples[index].tick, .threshold = threshold});
        }
    }
}

} // namespace

GuiSignalId bosch_lateral_error_signal_id() { return {GuiSignalScalarType::Real, "lateral_error"}; }

GuiSignalId bosch_critical_section_signal_id() {
    return {GuiSignalScalarType::Boolean, "critical_section"};
}

BoschResultAnalysis derive_bosch_result_analysis(const RunResult& result) {
    BoschResultAnalysis analysis;
    if (!result.signals.model.has_value() || !result.signals.diagnostics.empty()) {
        analysis.diagnostic = result.signals.diagnostics.empty()
                                  ? "Bosch signals are unavailable."
                                  : result.signals.diagnostics.front().message;
        return analysis;
    }
    const auto& model = result.signals.model.value();
    analysis.lateral_error = find_signal_series(model, bosch_lateral_error_signal_id());
    analysis.critical_section = find_signal_series(model, bosch_critical_section_signal_id());
    if (analysis.lateral_error == nullptr) {
        analysis.diagnostic = "Lateral error signal is unavailable for this Bosch run.";
    } else {
        add_crossings(*analysis.lateral_error, -0.2, analysis.threshold_crossings);
        add_crossings(*analysis.lateral_error, 0.2, analysis.threshold_crossings);
    }

    if (analysis.critical_section != nullptr) {
        std::optional<Tick> beginning;
        Tick last_true{};
        for (const auto& sample : analysis.critical_section->samples) {
            const auto active = std::get<bool>(sample.value);
            if (active) {
                if (!beginning.has_value()) {
                    beginning = sample.tick;
                }
                last_true = sample.tick;
            } else if (beginning.has_value()) {
                analysis.critical_intervals.push_back({*beginning, last_true});
                beginning.reset();
            }
        }
        if (beginning.has_value()) {
            analysis.critical_intervals.push_back({*beginning, last_true});
        }
    }
    for (const auto& event : result.snapshot.event_log) {
        if (event.type() == EventType::DeadlineMiss) {
            analysis.deadline_miss_ticks.push_back(event.tick());
        }
    }
    return analysis;
}

std::vector<WorkbookControlMetric> bosch_workbook_control_metrics(const RunResult& result) {
    const auto analysis = derive_bosch_result_analysis(result);
    std::vector<WorkbookControlMetric> metrics;
    metrics.reserve(analysis.threshold_crossings.size() + analysis.critical_intervals.size());
    for (const auto& crossing : analysis.threshold_crossings) {
        metrics.push_back({.metric = "lateral_error_threshold_crossing",
                           .tick = crossing.tick,
                           .value = std::to_string(crossing.threshold) + " m"});
    }
    for (const auto& interval : analysis.critical_intervals) {
        metrics.push_back({.metric = "critical_section_interval",
                           .tick = interval.begin_tick,
                           .value = "through tick " + std::to_string(interval.end_tick)});
    }
    return metrics;
}

} // namespace cpssim
