# Module Notes

Module notes are concise maintenance contracts. Use the
[project tour](../guide/PROJECT-TOUR.md) first when you need the overall story.

| Area | Module note |
|---|---|
| Core values and time | [Foundational types](foundational-types.md) |
| Immutable input | [Experiment configuration](experiment-configuration.md) |
| Jobs and resources | [Runtime state](runtime-state.md) |
| Event representation | [Canonical events](canonical-events.md) |
| Pending-event order | [Event queue](event-queue.md) |
| Task-driven releases | [Periodic releases](periodic-releases.md) |
| Scheduling | [Fixed-priority scheduling](fixed-priority-scheduling.md) |
| Multiple resources | [Multiple resources](multiple-resources.md) |
| Communication | [Causal messages](causal-messages.md) |
| Bosch timing oracle | [Bosch timing conformance](bosch-timing-conformance.md) |
| Bosch trigger projection | [Bosch trigger adapter](bosch-trigger-adapter.md) |
| FMI loading | [FMI 2.0 Co-Simulation](fmi2-co-simulation.md) |
| Online physical behavior | [Online functional interaction](online-functional-interaction.md) |
| CLI and shared run services | [Application interfaces](application-interfaces.md) |

Each page identifies responsibility, public interface, state ownership,
invariants, dependencies, failures, tests, and deeper references. Add or update
a module note when implementation changes one of those contracts.
