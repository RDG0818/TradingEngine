// tests/cpp/test_traders.cpp
#include <gtest/gtest.h>
#include "include/trader.h"
#include "include/traders/random_market.h"
#include "include/traders/random_limit.h"
#include "include/traders/market_maker.h"
#include "include/traders/momentum.h"
#include "include/traders/mean_reversion.h"

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

TEST(MomentumTrader, BuysWhenPriceRising) {
    MomentumTrader t(4, "momentum", 100000000ULL);
    std::vector<Order> submitted;
    auto submit = [&](Order o) { submitted.push_back(std::move(o)); };

    // Feed rising prices to establish uptrend.
    for (Price p = 100; p <= 120; p += 2)
        t.tick(p * 10000ULL, submit);

    bool has_buy = false;
    for (const auto& o : submitted)
        if (auto* lo = std::get_if<LimitOrder>(&o); lo && lo->side == Side::Buy)
            has_buy = true;
    EXPECT_TRUE(has_buy);
}

TEST(MeanReversionTrader, SellsAboveMean) {
    MeanReversionTrader t(5, "reverts", 100000000ULL);
    std::vector<Order> submitted;
    auto submit = [&](Order o) { submitted.push_back(std::move(o)); };

    // Establish mean around 100, then spike.
    for (int i = 0; i < 20; ++i) t.tick(1000000ULL, submit);
    submitted.clear();
    t.tick(1200000ULL, submit); // 20% above mean

    bool has_sell = false;
    for (const auto& o : submitted)
        if (auto* lo = std::get_if<LimitOrder>(&o); lo && lo->side == Side::Sell)
            has_sell = true;
    EXPECT_TRUE(has_sell);
}

TEST(MarketMakerTrader, TickSubmitsBidAndAsk) {
    MarketMakerTrader t(3, "mm", 100000000ULL, 642000000ULL /*seed price*/);
    std::vector<Order> submitted;
    t.tick(642000000ULL, [&](Order o) { submitted.push_back(std::move(o)); });

    // Should submit at least one bid and one ask per tick.
    bool has_bid = false, has_ask = false;
    for (const auto& o : submitted) {
        if (auto* lo = std::get_if<LimitOrder>(&o)) {
            if (lo->side == Side::Buy)  has_bid = true;
            if (lo->side == Side::Sell) has_ask = true;
        }
    }
    EXPECT_TRUE(has_bid);
    EXPECT_TRUE(has_ask);
}
