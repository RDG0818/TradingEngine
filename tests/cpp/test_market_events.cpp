// tests/cpp/test_market_events.cpp
#include <gtest/gtest.h>
#include "include/market_events.h"

TEST(MarketEvents, AllEventsHaveMetadata) {
    auto events = all_market_events();
    EXPECT_EQ(events.size(), 4u);
    for (const auto& e : events) {
        EXPECT_FALSE(e.id.empty());
        EXPECT_FALSE(e.name.empty());
        EXPECT_GT(e.default_duration_s, 0);
    }
}
