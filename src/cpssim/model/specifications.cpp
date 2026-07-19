/***
 * File: src/cpssim/model/specifications.cpp
 * Purpose: Implement construction-time validation for immutable resource and
 *          task specifications.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Invalid configuration is rejected before mutable simulator state is
 *        created.
 ***/

#include "cpssim/model/specifications.hpp"

#include <stdexcept>
#include <utility>

namespace cpssim {

/***
 * Stores the resource identity and takes ownership of its name after checking
 * that the name is not empty.
 ***/
ResourceSpec::ResourceSpec(ResourceId id, std::string name) : id_{id}, name_{std::move(name)} {
    if (name_.empty()) {
        throw std::invalid_argument{"resource name must not be empty"};
    }
}

/***
 * Stores one periodic-task specification and validates its local invariants:
 * positive period/deadline, nonnegative offset/priority, and a nonempty name.
 * Execution demand is validated later as part of a task-resource profile.
 ***/
TaskSpec::TaskSpec(TaskId id, std::string name, PeriodicTimingSpec timing, Priority priority)
    : id_{id}, name_{std::move(name)}, timing_{timing}, priority_{priority} {
    if (name_.empty()) {
        throw std::invalid_argument{"task name must not be empty"};
    }
    if (timing_.period <= 0) {
        throw std::invalid_argument{"task period must be positive"};
    }
    if (timing_.deadline <= 0) {
        throw std::invalid_argument{"task deadline must be positive"};
    }
    if (timing_.offset < 0) {
        throw std::invalid_argument{"task offset must not be negative"};
    }
    if (priority_ < 0) {
        throw std::invalid_argument{"task priority must not be negative"};
    }
}

} // namespace cpssim
