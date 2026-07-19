/***
 * File: tests/config/json_config_test.cpp
 * Purpose: Verify version-4 message-route configuration, legacy translation,
 *          strict rejection, and tracked example loading.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 ***/

#include "cpssim/config/json_config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace {

using cpssim::load_experiment_config;
using cpssim::parse_experiment_config;
using cpssim::PreemptionMode;
using cpssim::ResourceId;
using cpssim::TaskId;

/***
 * Derives the tracked T4 example path from this source file's repository
 * location so IDE parsing does not depend on a generated compile definition.
 ***/
std::filesystem::path tracked_example_path() {
    const auto repository_root =
        std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    return repository_root / "config" / "examples" / "basic.json";
}

/*** Verifies translation of a valid JSON document into validated model values. ***/
TEST_CASE("JSON creates a validated experiment configuration", "[config][json]") {
    const auto config = parse_experiment_config(R"json(
        {
          "schema_version": 3,
          "tick_period_ns": 100000,
          "scheduling": {"preemption": "non_preemptive"},
          "resources": [
            {"id": 1, "name": "local_cpu"}
          ],
          "tasks": [
            {
              "id": 1,
              "name": "sensor",
              "period_ticks": 50,
              "deadline_ticks": 50,
              "offset_ticks": 0,
              "priority": 1
            }
          ],
          "task_resource_profiles": [
            {
              "task_id": 1,
              "resource_id": 1,
              "execution_time": {
                "kind": "deterministic",
                "ticks": 6
              }
            }
          ]
        }
    )json");

    const auto values_match =
        config.tick_period() == std::chrono::nanoseconds{100'000} &&
        config.resources().front().id() == ResourceId{1} &&
        config.tasks().front().id() == TaskId{1} &&
        config.task_resource_profiles().front().execution_time == 6 &&
        config.scheduling().preemption_mode == PreemptionMode::NonPreemptive &&
        config.message_routes().empty();

    REQUIRE(values_match);
}

/*** Verifies schema-v4 translation of fixed task-to-task message routes. ***/
TEST_CASE("version-four JSON creates validated message routes", "[config][json][network]") {
    const auto config = parse_experiment_config(R"json(
        {
          "schema_version": 4,
          "tick_period_ns": 100000,
          "scheduling": {"preemption": "preemptive"},
          "resources": [{"id": 1, "name": "cpu"}],
          "tasks": [
            {"id": 1, "name": "producer", "period_ticks": 10,
             "deadline_ticks": 10, "offset_ticks": 0, "priority": 1},
            {"id": 2, "name": "consumer", "period_ticks": 20,
             "deadline_ticks": 20, "offset_ticks": 0, "priority": 2}
          ],
          "task_resource_profiles": [
            {"task_id": 1, "resource_id": 1,
             "execution_time": {"kind": "deterministic", "ticks": 1}},
            {"task_id": 2, "resource_id": 1,
             "execution_time": {"kind": "deterministic", "ticks": 2}}
          ],
          "message_routes": [
            {"source_task_id": 1, "destination_task_id": 2,
             "send_offset_ticks": 1, "delay_ticks": 80}
          ]
        }
    )json");

    const auto route_count_matches = config.message_routes().size() == 1;
    REQUIRE(route_count_matches);
    const auto& route = config.message_routes().front();
    const auto route_matches = route.source_task_id == TaskId{1} &&
                               route.destination_task_id == TaskId{2} && route.send_offset == 1 &&
                               route.delay == 80;
    REQUIRE(route_matches);
}

/*** Verifies rejection of malformed JSON, wrong versions, fields, and types. ***/
TEST_CASE("JSON validation rejects malformed or unsupported documents", "[config][json]") {
    REQUIRE_THROWS_AS(parse_experiment_config("{"), std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 5,
          "tick_period_ns": 100000,
          "resources": [],
          "tasks": []
        }
    )json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 100000,
          "resources": [],
          "tasks": [],
          "unexpected": true
        }
    )json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 100000,
          "resources": []
        }
    )json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 1.5,
          "resources": [],
          "tasks": []
        }
    )json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 18446744073709551615,
          "resources": [],
          "tasks": []
        }
    )json"),
                      std::invalid_argument);
}

/*** Verifies strict route shape, timing, endpoint, and presence rules in v4. ***/
TEST_CASE("version-four JSON rejects invalid message routes", "[config][json][network]") {
    const std::string common_prefix = R"json(
        {
          "schema_version": 4,
          "tick_period_ns": 100000,
          "scheduling": {"preemption": "preemptive"},
          "resources": [{"id": 1, "name": "cpu"}],
          "tasks": [
            {"id": 1, "name": "producer", "period_ticks": 10,
             "deadline_ticks": 10, "offset_ticks": 0, "priority": 1},
            {"id": 2, "name": "consumer", "period_ticks": 10,
             "deadline_ticks": 10, "offset_ticks": 0, "priority": 2}
          ],
          "task_resource_profiles": [
            {"task_id": 1, "resource_id": 1,
             "execution_time": {"kind": "deterministic", "ticks": 1}},
            {"task_id": 2, "resource_id": 1,
             "execution_time": {"kind": "deterministic", "ticks": 1}}
          ]
    )json";

    REQUIRE_THROWS_AS(parse_experiment_config(common_prefix + "}"), std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(common_prefix + R"json(,
          "message_routes": [
            {"source_task_id": 1, "destination_task_id": 2,
             "send_offset_ticks": 0, "delay_ticks": 1}
          ]
        })json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(common_prefix + R"json(,
          "message_routes": [
            {"source_task_id": 1, "destination_task_id": 3,
             "send_offset_ticks": 1, "delay_ticks": 1}
          ]
        })json"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(parse_experiment_config(common_prefix + R"json(,
          "message_routes": [
            {"source_task_id": 1, "destination_task_id": 2,
             "send_offset_ticks": 1, "delay_ticks": 1, "extra": true}
          ]
        })json"),
                      std::invalid_argument);
}

/*** Verifies that version 1 is translated into one task-resource profile. ***/
TEST_CASE("legacy version-one task mappings remain loadable", "[config][json]") {
    const auto config = parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{
            "id": 1,
            "name": "task",
            "resource_id": 1,
            "period_ticks": 10,
            "deadline_ticks": 10,
            "offset_ticks": 0,
            "priority": 1,
            "execution_time": {"kind": "deterministic", "ticks": 4}
          }]
        }
    )json");

    const auto translation_matches =
        config.tasks().front().id() == TaskId{1} &&
        config.task_resource_profiles().front().resource_id == ResourceId{1} &&
        config.task_resource_profiles().front().execution_time == 4 &&
        config.scheduling().preemption_mode == PreemptionMode::Preemptive &&
        config.message_routes().empty();
    REQUIRE(translation_matches);
}

/*** Verifies strict version-3 scheduling fields and preemption values. ***/
TEST_CASE("version-three JSON requires valid scheduling configuration", "[config][json]") {
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 3,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": [{"task_id": 1, "resource_id": 1,
            "execution_time": {"kind": "deterministic", "ticks": 1}}]
        }
    )json"),
                      std::invalid_argument);

    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 3,
          "tick_period_ns": 100000,
          "scheduling": {"preemption": "sometimes"},
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": [{"task_id": 1, "resource_id": 1,
            "execution_time": {"kind": "deterministic", "ticks": 1}}]
        }
    )json"),
                      std::invalid_argument);
}

/*** Verifies version-2 profile references and execution constraints. ***/
TEST_CASE("version-two JSON validates task-resource profiles", "[config][json]") {
    const auto legacy_v2 = parse_experiment_config(R"json(
        {
          "schema_version": 2,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": [{
            "task_id": 1,
            "resource_id": 1,
            "execution_time": {"kind": "deterministic", "ticks": 1}
          }]
        }
    )json");
    const bool legacy_mode_matches =
        legacy_v2.scheduling().preemption_mode == PreemptionMode::Preemptive &&
        legacy_v2.message_routes().empty();
    REQUIRE(legacy_mode_matches);

    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 2,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": []
        }
    )json"),
                      std::invalid_argument);

    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 2,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": [{
            "task_id": 1,
            "resource_id": 2,
            "execution_time": {"kind": "deterministic", "ticks": 1}
          }]
        }
    )json"),
                      std::invalid_argument);

    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 2,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{"id": 1, "name": "task", "period_ticks": 10,
                     "deadline_ticks": 10, "offset_ticks": 0, "priority": 1}],
          "task_resource_profiles": [{
            "task_id": 1,
            "resource_id": 1,
            "execution_time": {"kind": "deterministic", "ticks": 11}
          }]
        }
    )json"),
                      std::invalid_argument);
}

/*** Verifies rejection of unsupported execution kinds and missing resources. ***/
TEST_CASE("JSON validation rejects invalid execution and resource references", "[config][json]") {
    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{
            "id": 1,
            "name": "task",
            "resource_id": 2,
            "period_ticks": 10,
            "deadline_ticks": 10,
            "offset_ticks": 0,
            "priority": 1,
            "execution_time": {"kind": "random", "ticks": 1}
          }]
        }
    )json"),
                      std::invalid_argument);

    REQUIRE_THROWS_AS(parse_experiment_config(R"json(
        {
          "schema_version": 1,
          "tick_period_ns": 100000,
          "resources": [{"id": 1, "name": "local_cpu"}],
          "tasks": [{
            "id": 1,
            "name": "task",
            "resource_id": 2,
            "period_ticks": 10,
            "deadline_ticks": 10,
            "offset_ticks": 0,
            "priority": 1,
            "execution_time": {"kind": "deterministic", "ticks": 1}
          }]
        }
    )json"),
                      std::invalid_argument);
}

/*** Verifies tracked-file loading and clear failure for a missing file. ***/
TEST_CASE("the tracked example configuration loads through the file API", "[config][json]") {
    const auto config = load_experiment_config(tracked_example_path());
    const auto example_matches = config.resources().size() == 2 && config.tasks().size() == 2 &&
                                 config.task_resource_profiles().size() == 3 &&
                                 config.message_routes().size() == 1 &&
                                 config.scheduling().preemption_mode == PreemptionMode::Preemptive;

    REQUIRE(example_matches);
    REQUIRE_THROWS_AS(load_experiment_config("missing-t4-configuration.json"), std::runtime_error);
}

} // namespace
