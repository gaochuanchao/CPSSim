/*** Build the Bosch runtime resolver and completed-result analysis service. ***/
#include "cpssim/application/bosch_workbench_services.hpp"

#include "cpssim/analysis/run_result.hpp"
#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/bosch_result_analysis.hpp"

#include <chrono>
#include <memory>
#include <stop_token>
#include <utility>

namespace cpssim {

WorkbenchApplicationServices
make_bosch_workbench_services(const std::filesystem::path& reference_root,
                              const std::filesystem::path& shared_library) {
    return {.project_runtime_resolver =
                [reference_root, shared_library](const auto& root, const auto& metadata) {
                    return resolve_bosch_project_runtime(root, metadata, reference_root,
                                                         shared_library);
                },
            .completed_result_builder =
                [](const CompletedRunFinalizationRequest& request, std::stop_token stop) {
                    const auto started = std::chrono::steady_clock::now();
                    if (stop.stop_requested()) {
                        return CompletedRunResult{};
                    }
                    auto result = std::make_shared<const RunResult>(
                        build_run_result(request.data, request.scenario_kind));
                    std::shared_ptr<const BoschResultAnalysis> bosch_analysis;
                    if (!stop.stop_requested() && request.scenario_kind == "bosch") {
                        bosch_analysis = std::make_shared<const BoschResultAnalysis>(
                            derive_bosch_result_analysis(*result));
                    }
                    return CompletedRunResult{request.data->runtime_generation, std::move(result),
                                              std::move(bosch_analysis), request.performance,
                                              std::chrono::steady_clock::now() - started};
                }};
}

} // namespace cpssim
