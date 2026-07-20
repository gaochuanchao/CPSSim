/*** Focused XLSX convenience export layered over authoritative raw artifacts. ***/

#pragma once

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/application/project/project.hpp"
#include "cpssim/gui/selection_model.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cpssim {

inline constexpr std::uint64_t excel_maximum_rows = 1'048'576;

struct WorkbookDetailSheet {
    std::string name;
    std::uint64_t first_source_row{};
    std::uint64_t source_row_count{};

    bool operator==(const WorkbookDetailSheet&) const = default;
};

struct WorkbookControlMetric {
    std::string metric;
    std::optional<Tick> tick;
    std::string value;

    bool operator==(const WorkbookControlMetric&) const = default;
};

std::vector<WorkbookDetailSheet> plan_workbook_detail_sheets(std::string base_name,
                                                             std::uint64_t source_rows);

void write_results_workbook(const std::filesystem::path& path, const ProjectContext& project,
                            const RunResult& result, std::optional<GuiTickRange> range,
                            const std::vector<WorkbookControlMetric>& control_metrics = {});

} // namespace cpssim
