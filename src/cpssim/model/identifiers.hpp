/***
 * File: src/cpssim/model/identifiers.hpp
 * Purpose: Define distinct value types for stable simulator entity and event
 *          identifiers.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: Separate classes prevent accidental mixing of IDs that happen to
 *        contain the same integer. JobIdentity combines the two ID domains
 *        required to reference a task-local job unambiguously.
 ***/

#pragma once

#include <compare>
#include <cstdint>

namespace cpssim {

/*** Stable identifier for an immutable task specification. ***/
class TaskId {
  public:
    // Creates a task identifier from its stable unsigned numeric value.
    explicit TaskId(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric task identifier.
    std::uint64_t value() const { return value_; }

    // Compares task identifiers by their stored numeric values.
    auto operator<=>(const TaskId&) const = default;

  private:
    std::uint64_t value_;
};

/*** Task-local sequence number for one released job instance. ***/
class JobId {
  public:
    // Creates a task-local job number from its unsigned numeric value.
    explicit JobId(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric job number.
    std::uint64_t value() const { return value_; }

    // Compares task-local job numbers by their stored numeric values.
    auto operator<=>(const JobId&) const = default;

  private:
    std::uint64_t value_;
};

/***
 * Identifies one job unambiguously by combining its producing task with its
 * task-local job number.
 ***/
class JobIdentity {
  public:
    // Creates a complete job identity from its task and task-local job IDs.
    JobIdentity(TaskId task_id, JobId job_id) : task_id_{task_id}, job_id_{job_id} {}

    // Returns the task that produces this job sequence.
    TaskId task_id() const { return task_id_; }

    // Returns this job's number within its producing task.
    JobId job_id() const { return job_id_; }

    // Compares complete job identities by task ID and then task-local job ID.
    auto operator<=>(const JobIdentity&) const = default;

  private:
    TaskId task_id_;
    JobId job_id_;
};

/*** Stable identifier for an execution or communication resource. ***/
class ResourceId {
  public:
    // Creates a resource identifier from its stable unsigned numeric value.
    explicit ResourceId(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric resource identifier.
    std::uint64_t value() const { return value_; }

    // Compares resource identifiers by their stored numeric values.
    auto operator<=>(const ResourceId&) const = default;

  private:
    std::uint64_t value_;
};

/*** Stable identifier for a message passed between simulator entities. ***/
class MessageId {
  public:
    // Creates a message identifier from its stable unsigned numeric value.
    explicit MessageId(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric message identifier.
    std::uint64_t value() const { return value_; }

    // Compares message identifiers by their stored numeric values.
    auto operator<=>(const MessageId&) const = default;

  private:
    std::uint64_t value_;
};

/*** Stable identifier for a modeled vehicle. ***/
class VehicleId {
  public:
    // Creates a vehicle identifier from its stable unsigned numeric value.
    explicit VehicleId(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric vehicle identifier.
    std::uint64_t value() const { return value_; }

    // Compares vehicle identifiers by their stored numeric values.
    auto operator<=>(const VehicleId&) const = default;

  private:
    std::uint64_t value_;
};

/*** Stable insertion sequence assigned to a canonical event. ***/
class EventSequence {
  public:
    // Creates an event sequence from its stable insertion-order value.
    explicit EventSequence(std::uint64_t value) : value_{value} {}

    // Returns the stored numeric event sequence.
    std::uint64_t value() const { return value_; }

    // Compares event sequences by their stored numeric values.
    auto operator<=>(const EventSequence&) const = default;

  private:
    std::uint64_t value_;
};

} // namespace cpssim
