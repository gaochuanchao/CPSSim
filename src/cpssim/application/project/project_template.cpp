/*** Construct one small valid generic project without introducing a system editor. ***/

#include "cpssim/application/project/project_template.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace cpssim {

ProjectCreationRequest make_generic_project_template(std::filesystem::path parent_directory,
                                                     std::string name) {
    ExperimentConfig system{
        std::chrono::microseconds{100},
        SchedulingSpec{.preemption_mode = PreemptionMode::Preemptive},
        {ResourceSpec{ResourceId{1}, "CPU"}},
        {TaskSpec{TaskId{1}, "Task",
                  PeriodicTimingSpec{.period = 100, .deadline = 100, .offset = 0}, 1}},
        {TaskResourceProfile{
            .task_id = TaskId{1}, .resource_id = ResourceId{1}, .execution_time = 10}}};
    auto plan = build_run_plan(
        system,
        RunPlanRequest{.stop_tick = 1000,
                       .assignments = {{.task_id = TaskId{1}, .resource_id = ResourceId{1}}}});
    if (!plan.plan.has_value() || !plan.diagnostics.empty()) {
        throw std::logic_error{"built-in generic project template is invalid"};
    }
    return {.parent_directory = std::move(parent_directory),
            .name = std::move(name),
            .system = std::move(system),
            .default_run_plan = std::move(plan.plan.value()),
            .scenario_file = std::nullopt,
            .scenario_kind = "generic"};
}

} // namespace cpssim
