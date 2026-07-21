/*** Typed Bosch result-series derivation layered above generic run results. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/application/results_workbook.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cpssim {

struct BoschCriticalInterval {
    Tick begin_tick{};
    Tick end_tick{};

    bool operator==(const BoschCriticalInterval&) const = default;
};

struct BoschThresholdCrossing {
    Tick tick{};
    double threshold{};

    bool operator==(const BoschThresholdCrossing&) const = default;
};

struct BoschResultAnalysis {
    const GuiSignalSeries* lateral_error{};
    const GuiSignalSeries* critical_section{};
    std::vector<BoschCriticalInterval> critical_intervals;
    std::vector<BoschThresholdCrossing> threshold_crossings;
    std::vector<Tick> deadline_miss_ticks;
    std::optional<std::string> diagnostic;
};

GuiSignalId bosch_lateral_error_signal_id();
GuiSignalId bosch_critical_section_signal_id();
BoschResultAnalysis derive_bosch_result_analysis(const RunResult& result);
std::vector<BoschCriticalInterval>
visible_bosch_critical_intervals(const BoschResultAnalysis& analysis, Tick begin_tick,
                                 Tick end_tick);
std::vector<WorkbookControlMetric> bosch_workbook_control_metrics(const RunResult& result);

} // namespace cpssim
