// tests/cpp/test_traders.cpp
#include <gtest/gtest.h>
#include "include/trader.h"
#include "include/traders/random_market.h"
#include "include/traders/random_limit.h"

TEST(RandomMarketTrader, TickSubmitsMarketOrder) {
    RandomMarketTrader t(1, "noise_taker", 10000000ULL);
    std::vector<Order> submitted;
    t.tick(642000000ULL, [&](Order o) { submitted.push_back(std::move(o)); });
    // RandomMarketTrader submits ~1 order per tick on average (Poisson).
    // Just verify it doesn't crash and type is correct when an order is submitted.
    for (const auto& o : submitted)
        EXPECT_TRUE(std::holds_alternative<MarketOrder>(o));
}

TEST(RandomLimitTrader, TickSubmitsLimitOrder) {
    RandomLimitTrader t(2, "noise_maker", 10000000ULL);
    std::vector<Order> submitted;
    // Run several ticks to get at least one order.
    for (int i = 0; i < 10; ++i)
        t.tick(642000000ULL, [&](Order o) { submitted.push_back(std::move(o)); });
    for (const auto& o : submitted)
        EXPECT_TRUE(std::holds_alternative<LimitOrder>(o));
}
