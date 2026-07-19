/***
 * File: src/cpssim/policy/resource_allocator.cpp
 * Purpose: Implement single-resource and explicitly configured task
 *          placement strategies.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: File parsing remains outside this module. A caller may parse a plan
 *        and pass its validated records to ConfiguredResourceAllocator.
 ***/

#include "cpssim/policy/resource_allocator.hpp"

#include <stdexcept>

namespace cpssim {

/*** Validates accessibility and builds one deterministic assignment per task. ***/
std::vector<TaskAssignment>
SingleResourceAllocator::allocate(const ExperimentConfig& config) const {
    if (config.resources().size() != 1) {
        throw std::invalid_argument{"single-resource allocation requires exactly one resource"};
    }

    const auto resource_id = config.resources().front().id();
    std::vector<TaskAssignment> assignments;
    assignments.reserve(config.tasks().size());

    for (const auto& task : config.tasks()) {
        bool resource_is_accessible = false;
        for (const auto& profile : config.task_resource_profiles()) {
            if (profile.task_id == task.id() && profile.resource_id == resource_id) {
                resource_is_accessible = true;
                break;
            }
        }
        if (!resource_is_accessible) {
            throw std::invalid_argument{"task cannot execute on the single configured resource"};
        }
        assignments.push_back({.task_id = task.id(), .resource_id = resource_id});
    }
    return assignments;
}

/*** Copies an explicit plan so its lifetime is independent of the caller. ***/
ConfiguredResourceAllocator::ConfiguredResourceAllocator(
    const std::vector<TaskAssignment>& assignments)
    : assignments_{assignments} {}

/*** Returns the stored plan; the engine owns cross-record validation. ***/
std::vector<TaskAssignment> ConfiguredResourceAllocator::allocate(const ExperimentConfig&) const {
    return assignments_;
}

} // namespace cpssim
