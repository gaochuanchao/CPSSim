/***
 * File: src/cpssim/policy/fixed_priority.cpp
 * Purpose: Implement deterministic fixed-priority selection without mutating
 *          simulator runtime state.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Smaller numeric priority means higher priority. Equal priorities use
 *        stable timing and identity fields only.
 ***/

#include "cpssim/policy/fixed_priority.hpp"

#include <stdexcept>

namespace cpssim {

namespace {

/*** Finds one job by complete identity or reports an inconsistent scheduler view. ***/
const JobState& find_job(const std::vector<JobState>& jobs, JobIdentity identity) {
    for (const auto& job : jobs) {
        if (job.identity() == identity) {
            return job;
        }
    }
    throw std::logic_error{"ready job is absent from the scheduler job store"};
}

/*** Applies priority, release, task, and job ordering to two Ready jobs. ***/
bool comes_before(const JobState& left, const JobState& right) {
    if (left.priority() != right.priority()) {
        return left.priority() < right.priority();
    }
    if (left.release_tick() != right.release_tick()) {
        return left.release_tick() < right.release_tick();
    }
    if (left.task_id() != right.task_id()) {
        return left.task_id() < right.task_id();
    }
    return left.id() < right.id();
}

} // namespace

/***
 * Resolves every ready identity through the read-only job store and keeps the
 * smallest deterministic ordering key. No container order is used as a tie
 * breaker.
 ***/
JobIdentity FixedPriorityPolicy::select(const Resource&, const std::vector<JobIdentity>& ready_jobs,
                                        const std::vector<JobState>& jobs) const {
    if (ready_jobs.empty()) {
        throw std::logic_error{"fixed-priority policy received an empty ready queue"};
    }

    auto selected = ready_jobs.front();
    if (find_job(jobs, selected).lifecycle() != JobLifecycle::Ready) {
        throw std::logic_error{"scheduler ready membership contains a non-ready job"};
    }

    for (const auto identity : ready_jobs) {
        const auto& candidate = find_job(jobs, identity);
        if (candidate.lifecycle() != JobLifecycle::Ready) {
            throw std::logic_error{"scheduler ready membership contains a non-ready job"};
        }
        if (comes_before(candidate, find_job(jobs, selected))) {
            selected = identity;
        }
    }
    return selected;
}

/*** Implements the baseline rule that equal priority does not preempt. ***/
bool FixedPriorityPolicy::should_preempt(const JobState& running_job,
                                         const JobState& ready_job) const {
    return ready_job.priority() < running_job.priority();
}

} // namespace cpssim
