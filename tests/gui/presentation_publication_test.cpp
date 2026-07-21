/*** Verify coherent and rate-limited presentation snapshot publication. ***/
#include "cpssim/gui/presentation_publication.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cpssim;

TEST_CASE("Live publication is capped and semantic boundaries are immediate",
          "[gui][snapshot][generation]") {
    GuiPresentationPublicationPolicy policy;
    const auto start = std::chrono::steady_clock::time_point{std::chrono::seconds{1}};
    GuiPresentationPublicationInput input{.mode = GuiRunMode::Live,
                                          .update = {},
                                          .switched_fast_to_live = false,
                                          .missing_snapshot = true,
                                          .runtime_generation = 1,
                                          .simulation_data_generation = 1,
                                          .now = start};
    REQUIRE(policy.should_publish(input));
    policy.published(input);
    input.missing_snapshot = false;
    input.simulation_data_generation = 2;
    input.now += std::chrono::milliseconds{10};
    REQUIRE_FALSE(policy.should_publish(input));
    input.now += std::chrono::milliseconds{60};
    REQUIRE(policy.should_publish(input));
    policy.published(input);
    input.update.paused = true;
    input.now += std::chrono::milliseconds{1};
    REQUIRE(policy.should_publish(input));
}

TEST_CASE("Fast running data waits for a coherent publication boundary",
          "[gui][snapshot][generation]") {
    GuiPresentationPublicationPolicy policy;
    const auto now = std::chrono::steady_clock::now();
    GuiPresentationPublicationInput initial{.mode = GuiRunMode::Fast,
                                            .update = {},
                                            .switched_fast_to_live = false,
                                            .missing_snapshot = true,
                                            .runtime_generation = 3,
                                            .simulation_data_generation = 1,
                                            .now = now};
    policy.published(initial);
    auto next = initial;
    next.missing_snapshot = false;
    next.simulation_data_generation = 10;
    next.now += std::chrono::seconds{1};
    REQUIRE_FALSE(policy.should_publish(next));
    next.update.step_completed = true;
    REQUIRE(policy.should_publish(next));
}
