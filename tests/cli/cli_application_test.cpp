/***
 * File: tests/cli/cli_application_test.cpp
 * Purpose: Verify CPSSim command parsing, shell lifecycle, direct Bosch
 *          arguments, and injected wizard execution without a real terminal.
 * Creator: CPSSim contributors
 * Documentation date: 2026-07-20
 * Notes: Mock service execution keeps parser and prompt tests independent of
 *        the simulator, example CSV files, and FMI shared library.
 ***/

#include "apps/cli/cli_application.hpp"
#include "apps/cli/command_parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class MockBoschRunService final : public cpssim::BoschRunService {
  public:
    cpssim::BoschRunSummary run(const cpssim::BoschRunRequest& request) const override {
        requests.push_back(request);
        if (failure.has_value()) {
            throw std::runtime_error{*failure};
        }
        if (request.stop_tick.has_value() && *request.stop_tick > maximum_stop_tick) {
            throw std::invalid_argument{"stop tick exceeds the final supplied example tick"};
        }
        return {.example_directory = request.example_directory,
                .scenario = request.scenario,
                .stop_tick = request.stop_tick.value_or(maximum_stop_tick),
                .canonical_event_count = 12,
                .functional_observation_count = 4,
                .final_observation = cpssim::FunctionalObservation{
                    .tick = 3, .real_signals = {}, .integer_signals = {}, .boolean_signals = {}}};
    }

    mutable std::vector<cpssim::BoschRunRequest> requests;
    std::optional<std::string> failure;
    cpssim::Tick maximum_stop_tick{1'499'999};
};

struct CliFixture {
    explicit CliFixture(std::string input_text = {})
        : input{std::move(input_text)},
          application{{.input = input, .output = output, .error = error},
                      service,
                      {.repository_root = "/repository",
                       .fmu_shared_library = "/build/LateralMotionControl.so"}} {}

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;
    MockBoschRunService service;
    cpssim::CliApplication application;
};

} // namespace

TEST_CASE("CLI command parser handles whitespace quotes and empty arguments", "[cli][parser]") {
    REQUIRE(cpssim::parse_cli_command_line("  run bosch --example 'examples/example_v_10' ") ==
            std::vector<std::string>{"run", "bosch", "--example", "examples/example_v_10"});
    REQUIRE(cpssim::parse_cli_command_line("help \"\"") == std::vector<std::string>{"help", ""});
    REQUIRE_THROWS_AS(cpssim::parse_cli_command_line("run 'bosch"), std::invalid_argument);
}

TEST_CASE("CLI shell accepts empty input and exits cleanly on EOF", "[cli][shell][eof]") {
    CliFixture fixture{"\n"};

    REQUIRE(fixture.application.run({}) == 0);
    REQUIRE(fixture.output.str().find("Interactive terminal interface") != std::string::npos);
    REQUIRE(fixture.output.str().find("cpssim> cpssim> ") != std::string::npos);
    REQUIRE(fixture.error.str().empty());
}

TEST_CASE("CLI shell reports an unknown command and continues", "[cli][shell][error]") {
    CliFixture fixture{"unknown\nexit\n"};

    REQUIRE(fixture.application.run({}) == 0);
    REQUIRE(fixture.error.str().find("Unknown command: unknown") != std::string::npos);
}

TEST_CASE("CLI help and version commands expose registered metadata", "[cli][commands]") {
    CliFixture fixture;

    REQUIRE(fixture.application.run({"help"}) == 0);
    REQUIRE(fixture.output.str().find("run bosch") != std::string::npos);
    REQUIRE(fixture.application.run({"version"}) == 0);
    REQUIRE(fixture.output.str().find("CPSSim 0.1.0") != std::string::npos);
}

TEST_CASE("CLI quit and exit both close the interactive shell", "[cli][shell][exit]") {
    CliFixture quit_fixture{"quit\n"};
    CliFixture exit_fixture{"exit\n"};

    REQUIRE(quit_fixture.application.run({}) == 0);
    REQUIRE(exit_fixture.application.run({}) == 0);
}

TEST_CASE("direct Bosch arguments call the shared service once", "[cli][bosch][direct]") {
    CliFixture fixture;

    REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/example_v_10",
                                     "--scenario", "shared_cloud", "--stop-tick", "150000"}) == 0);
    REQUIRE(fixture.service.requests.size() == 1);
    const auto& request = fixture.service.requests.front();
    REQUIRE(request.example_directory ==
            std::filesystem::path{"/repository/examples/example_v_10"});
    REQUIRE(request.scenario == cpssim::BoschReferenceScenario::SharedCloud);
    REQUIRE(request.stop_tick == 150000);
    REQUIRE(fixture.output.str().find("Canonical events: 12") != std::string::npos);
}

TEST_CASE("direct Bosch accepts every supplied trajectory and scenario combination",
          "[cli][bosch][direct][choices]") {
    constexpr std::array examples{"example_v_10", "example_v_12_5", "example_v_15"};
    constexpr std::array scenarios{"dedicated", "shared_cloud"};

    for (const auto* example : examples) {
        for (const auto* scenario : scenarios) {
            CAPTURE(example, scenario);
            CliFixture fixture;
            REQUIRE(fixture.application.run({"run", "bosch", "--example",
                                             std::string{"examples/"} + example, "--scenario",
                                             scenario, "--stop-tick", "2"}) == 0);
            REQUIRE(fixture.service.requests.size() == 1);
            REQUIRE(fixture.service.requests.front().example_directory.filename() == example);
            REQUIRE(cpssim::bosch_reference_scenario_name(
                        fixture.service.requests.front().scenario) == scenario);
        }
    }
}

TEST_CASE("direct Bosch parsing rejects invalid selections", "[cli][bosch][validation]") {
    SECTION("trajectory") {
        CliFixture fixture;
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/unknown",
                                         "--scenario", "dedicated"}) == 2);
        REQUIRE(fixture.service.requests.empty());
    }
    SECTION("trajectory outside supplied directory") {
        CliFixture fixture;
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "/tmp/example_v_10",
                                         "--scenario", "dedicated"}) == 2);
        REQUIRE(fixture.service.requests.empty());
    }
    SECTION("scenario") {
        CliFixture fixture;
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/example_v_10",
                                         "--scenario", "unknown"}) == 2);
        REQUIRE(fixture.service.requests.empty());
    }
    SECTION("stop tick") {
        CliFixture fixture;
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/example_v_10",
                                         "--scenario", "dedicated", "--stop-tick", "-1"}) == 2);
        REQUIRE(fixture.service.requests.empty());
    }
}

TEST_CASE("Bosch service failures make direct commands fail", "[cli][bosch][failure]") {
    SECTION("stop tick beyond data") {
        CliFixture fixture;
        fixture.service.maximum_stop_tick = 10;
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/example_v_12_5",
                                         "--scenario", "dedicated", "--stop-tick", "11"}) == 2);
        REQUIRE(fixture.error.str().find("exceeds the final supplied example tick") !=
                std::string::npos);
    }
    SECTION("general execution failure") {
        CliFixture fixture;
        fixture.service.failure = "mock FMI failure";
        REQUIRE(fixture.application.run({"run", "bosch", "--example", "examples/example_v_15",
                                         "--scenario", "shared_cloud"}) == 2);
        REQUIRE(fixture.error.str().find("mock FMI failure") != std::string::npos);
    }
}

TEST_CASE("interactive Bosch wizard calls the same injected service", "[cli][bosch][wizard]") {
    CliFixture fixture{"run bosch\n1\n2\n2\n15\nexit\n"};

    REQUIRE(fixture.application.run({}) == 0);
    REQUIRE(fixture.service.requests.size() == 1);
    const auto& request = fixture.service.requests.front();
    REQUIRE(request.example_directory.filename() == "example_v_10");
    REQUIRE(request.scenario == cpssim::BoschReferenceScenario::SharedCloud);
    REQUIRE(request.stop_tick == 15);
    REQUIRE(fixture.output.str().find("Run summary") != std::string::npos);
}

TEST_CASE("interactive Bosch wizard retries malformed numbered choices",
          "[cli][bosch][wizard][validation]") {
    CliFixture fixture{"run bosch\ninvalid\n1\n1\n1\nexit\n"};

    REQUIRE(fixture.application.run({}) == 0);
    REQUIRE(fixture.service.requests.size() == 1);
    REQUIRE(fixture.error.str().find("Choose a number from 1 to 3.") != std::string::npos);
}

TEST_CASE("interactive Bosch wizard reports EOF without calling the service",
          "[cli][bosch][wizard][eof]") {
    CliFixture fixture{"run bosch\n"};

    REQUIRE(fixture.application.run({}) == 0);
    REQUIRE(fixture.service.requests.empty());
    REQUIRE(fixture.error.str().find("input ended during the Bosch wizard") != std::string::npos);
}
