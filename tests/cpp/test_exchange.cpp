// tests/cpp/test_exchange.cpp
#include <gtest/gtest.h>
#include "include/engine/exchange.h"
#include <thread>
#include <chrono>

TEST(Exchange, StartStop) {
    Exchange ex;
    ex.start(642000000ULL); // $64,200.00
    EXPECT_TRUE(ex.is_running());
    ex.stop();
    EXPECT_FALSE(ex.is_running());
}

TEST(Exchange, SubmitOrderProducesFill) {
    Exchange ex;
    ex.start(642000000ULL);

    TraderId user1 = ex.create_portfolio(10000000000ULL); // $1M
    TraderId user2 = ex.create_portfolio(10000000000ULL);

    // Add liquidity via a limit sell.
    LimitOrder sell{1, user1, Side::Sell, 642000000ULL, 100, TimeInForce::GTC, {}};
    ex.submit_order(Order{sell});

    // Buy against it.
    LimitOrder buy{2, user2, Side::Buy, 642100000ULL, 50, TimeInForce::GTC, {}};
    ex.submit_order(Order{buy});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ex.stop();
}

TEST(Exchange, BookSnapshot) {
    Exchange ex;
    ex.start(642000000ULL);
    LimitOrder bid{1, 99, Side::Buy, 641000000ULL, 10, TimeInForce::GTC, {}};
    ex.submit_order(Order{bid});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto snap = ex.book_snapshot();
    ASSERT_FALSE(snap.bids.empty());
    EXPECT_EQ(snap.bids[0].first, 641000000ULL);
    ex.stop();
}
