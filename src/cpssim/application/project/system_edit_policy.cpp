/*** Enforce adapter-safe structural invariants before canonical reconstruction. ***/

#include "cpssim/application/project/system_edit_policy.hpp"

#include <algorithm>
#include <array>

namespace cpssim {

ProjectSystemEditPolicy project_system_edit_policy(const ProjectMetadata& metadata) noexcept {
    if (metadata.scenario_kind == "generic")
        return ProjectSystemEditPolicy::Generic;
    if (metadata.scenario_kind == "bosch")
        return ProjectSystemEditPolicy::BoschCompatible;
    return ProjectSystemEditPolicy::ReadOnlyAdapter;
}

std::vector<std::string> validate_project_system_edit_policy(const ProjectContext& project,
                                                             const EditableSystemDraft& draft) {
    const auto policy = project_system_edit_policy(project.metadata());
    if (policy == ProjectSystemEditPolicy::Generic)
        return {};
    if (policy == ProjectSystemEditPolicy::ReadOnlyAdapter)
        return draft.dirty() ? std::vector<std::string>{"adapter-owned systems are read-only"}
                             : std::vector<std::string>{};
    static constexpr std::array<std::string_view, 6> names{"Sensor",      "Estimator", "Controller",
                                                           "Feedforward", "Merger",    "Actuator"};
    std::vector<std::string> diagnostics;
    if (draft.tasks().size() != names.size())
        diagnostics.push_back("Bosch projects require exactly tasks T1-T6");
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto id = TaskId{index + 1};
        const auto found = std::find_if(draft.tasks().begin(), draft.tasks().end(),
                                        [id](const auto& task) { return task.id == id; });
        if (found == draft.tasks().end() || found->name != names[index])
            diagnostics.push_back("Bosch task T" + std::to_string(id.value()) +
                                  " identity is fixed as " + std::string{names[index]});
    }
    const auto required_route = [&draft](TaskId source, TaskId destination) {
        return std::count_if(draft.routes().begin(), draft.routes().end(), [&](const auto& route) {
                   return route.source_task_id == source &&
                          route.destination_task_id == destination;
               }) == 1;
    };
    if (draft.routes().size() != 2 || !required_route(TaskId{1}, TaskId{2}) ||
        !required_route(TaskId{5}, TaskId{6}))
        diagnostics.push_back("Bosch network route endpoints Sensor -> Estimator and Merger -> "
                              "Actuator are protected");
    return diagnostics;
}

} // namespace cpssim
