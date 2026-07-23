# Core Symbol Index

This index maps important behavior-bearing symbols to declaration,
implementation, primary use, and evidence. Inline accessors and trivial value
comparisons are intentionally not listed individually.

## Time and identifiers

| Symbol | Declaration | Definition | Used by | Evidence |
|---|---|---|---|---|
| `Tick` | [`time.hpp`](../../../src/cpssim/model/time.hpp) | alias in header | every canonical timestamp | [`time_test.cpp`](../../../tests/model/time_test.cpp) |
| `duration_to_ticks` | [`time.hpp`](../../../src/cpssim/model/time.hpp) | [`time.cpp`](../../../src/cpssim/model/time.cpp) | config/adapters | `time_test.cpp` |
| `ticks_to_duration` | `time.hpp` | `time.cpp` | results/FMI boundaries | `time_test.cpp` |
| `TaskId`, `JobId`, `ResourceId`, `MessageId` | [`identifiers.hpp`](../../../src/cpssim/model/identifiers.hpp) | inline | all modules | model tests |
| `JobIdentity` | `identifiers.hpp` | inline | scheduler/GUI selection | scheduler tests |
| `EventSequence` | `identifiers.hpp` | inline | event queue/causality | event queue tests |

## Immutable model

| Symbol | Declaration | Definition | Primary use |
|---|---|---|---|
| `ResourceSpec` | [`specifications.hpp`](../../../src/cpssim/model/specifications.hpp) | [`specifications.cpp`](../../../src/cpssim/model/specifications.cpp) | build runtime Resource |
| `TaskSpec` | `specifications.hpp` | `specifications.cpp` | build runtime Task |
| `TaskResourceProfile` | `specifications.hpp` | aggregate | accessibility/execution demand |
| `MessageRouteSpec` | `specifications.hpp` | aggregate | fixed-delay/logical links |
| `ExperimentConfig` constructor | [`experiment_config.hpp`](../../../src/cpssim/model/experiment_config.hpp) | [`experiment_config.cpp`](../../../src/cpssim/model/experiment_config.cpp) | cross-record validation |
| `RunPlan` | [`run_plan.hpp`](../../../src/cpssim/model/run_plan.hpp) | [`run_plan.cpp`](../../../src/cpssim/model/run_plan.cpp) | accepted per-run choices |
| `build_run_plan` | `run_plan.hpp` | `run_plan.cpp` | GUI/session construction |

## JSON configuration

| Symbol | Declaration | Definition | Role |
|---|---|---|---|
| `parse_experiment_config` | [`json_config.hpp`](../../../src/cpssim/config/json_config.hpp) | [`json_config.cpp`](../../../src/cpssim/config/json_config.cpp) | strict text parser |
| `serialize_experiment_config_json` | `json_config.hpp` | `json_config.cpp` | canonical schema-v4 output |
| `load_experiment_config` | `json_config.hpp` | `json_config.cpp` | file read + parse |
| `save_experiment_config` | `json_config.hpp` | `json_config.cpp` | canonical file write |
| `parse_document_v1..v4` | private in `json_config.cpp` | same | explicit compatibility |
| `parse_message_route` | private in `json_config.cpp` | same | v4 route translation |

## Events and queue

| Symbol | Declaration | Definition | Caller/consumer | Evidence |
|---|---|---|---|---|
| `Event` constructor | [`event.hpp`](../../../src/cpssim/model/event.hpp) | [`event.cpp`](../../../src/cpssim/model/event.cpp) | `EventQueue::schedule` | model/event tests |
| `EventQueue::schedule` | [`event_queue.hpp`](../../../src/cpssim/kernel/event_queue.hpp) | [`event_queue.cpp`](../../../src/cpssim/kernel/event_queue.cpp) | event producers | [`event_queue_test.cpp`](../../../tests/kernel/event_queue_test.cpp) |
| `EventQueue::next` | `event_queue.hpp` | `event_queue.cpp` | `SimulationEngine` | event queue tests |
| `EventQueue::pop_next` | `event_queue.hpp` | `event_queue.cpp` | `SimulationEngine` | event queue tests |
| `EventQueue::LaterEvent::operator()` | private in `event_queue.hpp` | `event_queue.cpp` | priority queue | event queue tests |
| `phase_precedence` | private in `event_queue.cpp` | same | comparator/input validation | event queue tests |

## Runtime task and release

| Symbol | Declaration | Definition | Used by | Evidence |
|---|---|---|---|---|
| `Task::Task` | [`periodic_release.hpp`](../../../src/cpssim/kernel/periodic_release.hpp) | [`periodic_release.cpp`](../../../src/cpssim/kernel/periodic_release.cpp) | `PeriodicReleaseModel` | [`periodic_release_test.cpp`](../../../tests/kernel/periodic_release_test.cpp) |
| `Task::assign_resource` | `periodic_release.hpp` | `periodic_release.cpp` | engine allocation | release tests |
| `Task::execution_time_on` | `periodic_release.hpp` | `periodic_release.cpp` | job construction | release tests |
| `Task::schedule_initial_release` | `periodic_release.hpp` | `periodic_release.cpp` | model initialization | release tests |
| `Task::release` | `periodic_release.hpp` | `periodic_release.cpp` | engine release phase | release/engine tests |
| `Task::schedule_current_release` | private | `periodic_release.cpp` | `Task::release` | release tests |
| `PeriodicReleaseModel::PeriodicReleaseModel` | `periodic_release.hpp` | `periodic_release.cpp` | engine constructor | engine tests |
| `PeriodicReleaseModel::assign_resource` | `periodic_release.hpp` | `periodic_release.cpp` | engine allocation | release tests |
| `PeriodicReleaseModel::schedule_initial_releases` | `periodic_release.hpp` | `periodic_release.cpp` | engine initialize | release/engine tests |
| `PeriodicReleaseModel::release` | `periodic_release.hpp` | `periodic_release.cpp` | engine release event | engine tests |

## Job and resource state

| Symbol | Declaration | Definition | Used by | Evidence |
|---|---|---|---|---|
| `JobState::JobState` | [`runtime_state.hpp`](../../../src/cpssim/model/runtime_state.hpp) | [`runtime_state.cpp`](../../../src/cpssim/model/runtime_state.cpp) | Task release | [`runtime_state_test.cpp`](../../../tests/model/runtime_state_test.cpp) |
| `JobState::mark_deadline_missed` | `runtime_state.hpp` | `runtime_state.cpp` | scheduler deadline phase | runtime/scheduler tests |
| `Resource::Resource` | `runtime_state.hpp` | `runtime_state.cpp` | Scheduler constructor | runtime tests |
| `Resource::start_job` | `runtime_state.hpp` | `runtime_state.cpp` | scheduler dispatch | runtime/scheduler tests |
| `Resource::preempt_job` | `runtime_state.hpp` | `runtime_state.cpp` | scheduler preemption | runtime/scheduler tests |
| `Resource::charge_execution` | `runtime_state.hpp` | `runtime_state.cpp` | completion/preemption | runtime/scheduler tests |
| `Resource::busy_ticks_until` | `runtime_state.hpp` | `runtime_state.cpp` | snapshots/results | runtime/result tests |
| `Resource::idle_ticks_until` | `runtime_state.hpp` | `runtime_state.cpp` | snapshots/results | runtime/result tests |
| `Resource::require_matching_resource` | private | `runtime_state.cpp` | transition validation | runtime tests |

## Scheduling policy and allocator

| Symbol | Declaration | Definition | Used by |
|---|---|---|---|
| `ResourceAllocator::allocate` | [`resource_allocator.hpp`](../../../src/cpssim/policy/resource_allocator.hpp) | concrete allocator `.cpp` | engine construction |
| `ConfiguredResourceAllocator` | `resource_allocator.hpp` | [`resource_allocator.cpp`](../../../src/cpssim/policy/resource_allocator.cpp) | GUI/run plan |
| `SchedulingPolicy::observe` | [`scheduling_policy.hpp`](../../../src/cpssim/policy/scheduling_policy.hpp) | virtual default | scheduler |
| `SchedulingPolicy::select` | `scheduling_policy.hpp` | concrete policy | scheduler |
| `SchedulingPolicy::should_preempt` | `scheduling_policy.hpp` | concrete policy | scheduler |
| `FixedPriorityPolicy::select` | [`fixed_priority.hpp`](../../../src/cpssim/policy/fixed_priority.hpp) | [`fixed_priority.cpp`](../../../src/cpssim/policy/fixed_priority.cpp) | `Scheduler::schedule` |
| `FixedPriorityPolicy::should_preempt` | `fixed_priority.hpp` | `fixed_priority.cpp` | `Scheduler::schedule` |

## Scheduler

| Symbol | Declaration | Definition | Role |
|---|---|---|---|
| `Scheduler::Scheduler` | [`scheduler.hpp`](../../../src/cpssim/kernel/scheduler.hpp) | [`scheduler.cpp`](../../../src/cpssim/kernel/scheduler.cpp) | build sorted resource domain |
| `Scheduler::observe` | `scheduler.hpp` | `scheduler.cpp` | forward functional context |
| `Scheduler::submit` | `scheduler.hpp` | `scheduler.cpp` | own new job/Ready/deadline |
| `Scheduler::process_completion` | `scheduler.hpp` | `scheduler.cpp` | validate stale/complete |
| `Scheduler::process_deadline` | `scheduler.hpp` | `scheduler.cpp` | record miss |
| `Scheduler::schedule` | `scheduler.hpp` | `scheduler.cpp` | one cycle over resources |
| `Scheduler::resource_state` | private | `scheduler.cpp` | resolve resource owner |
| `Scheduler::find_job` | private | `scheduler.cpp` | resolve job store |
| `Scheduler::has_active_job` | private | `scheduler.cpp` | reject overlap |
| `Scheduler::start_selected_job` | private | `scheduler.cpp` | apply start/resume candidates |

Closest evidence:
[`scheduler_test.cpp`](../../../tests/kernel/scheduler_test.cpp).

## Messages and network

| Symbol | Declaration | Definition | Role |
|---|---|---|---|
| `Message` | [`message.hpp`](../../../src/cpssim/model/message.hpp) | [`message.cpp`](../../../src/cpssim/model/message.cpp) | lifecycle value |
| `FixedDelayNetwork::FixedDelayNetwork` | [`fixed_delay_network.hpp`](../../../src/cpssim/network/fixed_delay_network.hpp) | [`fixed_delay_network.cpp`](../../../src/cpssim/network/fixed_delay_network.cpp) | validate/order routes |
| `FixedDelayNetwork::publish` | `fixed_delay_network.hpp` | `fixed_delay_network.cpp` | completion -> PendingSend |
| `FixedDelayNetwork::process_send` | `fixed_delay_network.hpp` | `fixed_delay_network.cpp` | PendingSend -> InFlight |
| `FixedDelayNetwork::process_delivery` | `fixed_delay_network.hpp` | `fixed_delay_network.cpp` | InFlight -> Delivered |
| `FixedDelayNetwork::find_message` | private | `fixed_delay_network.cpp` | resolve owned message |
| `FixedDelayNetwork::allocate_message_id` | private | `fixed_delay_network.cpp` | stable one-based IDs |

## Functional boundary

| Symbol | Declaration | Definition | Role |
|---|---|---|---|
| `FunctionalModel::initialize` | [`functional_model.hpp`](../../../src/cpssim/functional/functional_model.hpp) | adapter | initial observation |
| `FunctionalModel::advance_to` | `functional_model.hpp` | adapter | integer-tick progression |
| `FunctionalModel::apply_actions` | `functional_model.hpp` | adapter | accepted event batch |
| `FunctionalModel::finalize` | `functional_model.hpp` | adapter | terminate model |
| `FunctionalRuntime` | [`functional_runtime.hpp`](../../../src/cpssim/functional/functional_runtime.hpp) | [`functional_runtime.cpp`](../../../src/cpssim/functional/functional_runtime.cpp) | lifecycle/trace validation |

## Simulation engine

| Symbol | Declaration | Definition | Role |
|---|---|---|---|
| `SimulationEngine` constructors | [`simulation_engine.hpp`](../../../src/cpssim/kernel/simulation_engine.hpp) | [`simulation_engine.cpp`](../../../src/cpssim/kernel/simulation_engine.cpp) | compose runtime/optional model |
| `run` | `simulation_engine.hpp` | `simulation_engine.cpp` | process all event ticks once |
| `step_to_next_event` | `simulation_engine.hpp` | `simulation_engine.cpp` | one complete next tick |
| `initialize` | private | `simulation_engine.cpp` | initial releases/model |
| `finalize` | private | `simulation_engine.cpp` | horizon/model close |
| `process_event_tick` | private | `simulation_engine.cpp` | full semantic cycle |
| `apply_assignments` | private | `simulation_engine.cpp` | validate allocator plan |
| `process_pre_scheduling_event` | private | `simulation_engine.cpp` | completion/delivery/deadline/release |
| `process_release_batch` | private | `simulation_engine.cpp` | semantic release order |
| `process_post_scheduling_events` | private | `simulation_engine.cpp` | scheduling observations/sends |
| `forward_functional_observations` | private | `simulation_engine.cpp` | policy context |

Closest evidence:
[`simulation_engine_test.cpp`](../../../tests/kernel/simulation_engine_test.cpp).
