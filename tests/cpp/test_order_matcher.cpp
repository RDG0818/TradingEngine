// tests/cpp/test_order_matcher.cpp
#include <gtest/gtest.h>
#include "include/engine/order_matcher.h"
#include "include/engine/exchange_events.h"
#include <thread>
#include <chrono>

static LimitOrder make_limit(OrderId id, TraderId tid, Side side, Price price, Quantity qty,
                              TimeInForce tif = TimeInForce::GTC) {
    return {id, tid, side, price, qty, tif, {}};
}

// Helper: submit order and wait briefly for worker to process.
static void submit_and_wait(OrderMatcher& m, Order o, int ms = 20) {
    m.submit(std::move(o));
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

TEST(OrderMatcher, NonMatchingLimitRestInBook) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    bool accepted = false;
    bus.subscribe<OrderAcceptedEvent>([&](const OrderAcceptedEvent& e) {
        if (e.order_id == 1) accepted = true;
    });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy, 100, 50));
    EXPECT_TRUE(accepted);
    EXPECT_EQ(*matcher.book().best_bid(), 100ULL);

    matcher.stop();
}

TEST(OrderMatcher, MatchingLimitsProduceFill) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy,  100, 50));
    submit_and_wait(matcher, make_limit(2, 11, Side::Sell, 100, 50));

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_price, 100ULL);
    EXPECT_EQ(fills[0].fill_qty,    50ULL);
    EXPECT_EQ(fills[0].maker_order_id, 1ULL);
    EXPECT_EQ(fills[0].taker_order_id, 2ULL);
    // Book should be empty after full match.
    EXPECT_FALSE(matcher.book().best_bid().has_value());
    EXPECT_FALSE(matcher.book().best_ask().has_value());

    matcher.stop();
}

TEST(OrderMatcher, PartialFillLeavesRemainder) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy,  100, 100));
    submit_and_wait(matcher, make_limit(2, 11, Side::Sell, 100,  40));

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_qty, 40ULL);
    // 60 units should remain on the bid side.
    ASSERT_TRUE(matcher.book().best_bid().has_value());

    matcher.stop();
}

TEST(OrderMatcher, SelfMatchPrevented) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });

    // Same trader on both sides.
    submit_and_wait(matcher, make_limit(1, 10, Side::Buy,  100, 50));
    submit_and_wait(matcher, make_limit(2, 10, Side::Sell, 100, 50));

    EXPECT_TRUE(fills.empty());

    matcher.stop();
}

TEST(OrderMatcher, FOKFullyFillsWhenLiquiditySufficient) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });

    submit_and_wait(matcher, make_limit(1, 10, Side::Sell, 100, 50));
    submit_and_wait(matcher, make_limit(2, 11, Side::Buy, 100, 50, TimeInForce::FOK));

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_qty, 50ULL);
    EXPECT_FALSE(matcher.book().best_ask().has_value());

    matcher.stop();
}

TEST(OrderMatcher, FOKRejectsWithZeroFillsWhenLiquidityInsufficient) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bool rejected = false;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });
    bus.subscribe<OrderRejectedEvent>([&](const OrderRejectedEvent& e) {
        if (e.order_id == 2) rejected = true;
    });

    // Only 30 units resting; FOK taker wants 50 — must reject with zero fills.
    submit_and_wait(matcher, make_limit(1, 10, Side::Sell, 100, 30));
    submit_and_wait(matcher, make_limit(2, 11, Side::Buy, 100, 50, TimeInForce::FOK));

    EXPECT_TRUE(fills.empty());
    EXPECT_TRUE(rejected);
    // The resting sell order must be untouched — no partial execution.
    ASSERT_TRUE(matcher.book().best_ask().has_value());
    EXPECT_EQ(*matcher.book().best_ask(), 100ULL);

    matcher.stop();
}

TEST(OrderMatcher, FOKRejectsWithNoLiquidityAtAll) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    bool rejected = false;
    bus.subscribe<OrderRejectedEvent>([&](const OrderRejectedEvent& e) {
        if (e.order_id == 1) rejected = true;
    });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy, 100, 50, TimeInForce::FOK));

    EXPECT_TRUE(rejected);
    EXPECT_FALSE(matcher.book().best_bid().has_value());

    matcher.stop();
}

TEST(OrderMatcher, FOKRejectsWhenLevelTotalIsStaleFromPartialFill) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bool rejected = false;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });
    bus.subscribe<OrderRejectedEvent>([&](const OrderRejectedEvent& e) {
        if (e.order_id == 3) rejected = true;
    });

    // Rest a sell order for 50, from trader 10.
    submit_and_wait(matcher, make_limit(1, 10, Side::Sell, 100, 50));
    // Partially fill it with an IOC buy for 30, from trader 11 — leaves 20
    // truly available, but PriceLevel::total_qty at price 100 is still 50
    // because partial fills don't touch total_qty (only full cancel does).
    submit_and_wait(matcher, make_limit(2, 11, Side::Buy, 100, 30, TimeInForce::IOC));
    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_qty, 30ULL);
    fills.clear();

    // A FOK buy for 50 from a third trader must reject with zero fills:
    // real remaining liquidity at price 100 is only 20, not the stale
    // level total of 50.
    submit_and_wait(matcher, make_limit(3, 12, Side::Buy, 100, 50, TimeInForce::FOK));

    EXPECT_TRUE(fills.empty());
    EXPECT_TRUE(rejected);
    // The resting order's remaining 20 units must be untouched.
    ASSERT_TRUE(matcher.book().best_ask().has_value());
    EXPECT_EQ(*matcher.book().best_ask(), 100ULL);

    matcher.stop();
}

TEST(OrderMatcher, CancelRemovesOrder) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    bool cancelled = false;
    bus.subscribe<OrderCancelledEvent>([&](const OrderCancelledEvent& e) {
        if (e.order_id == 1) cancelled = true;
    });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy, 100, 50));
    matcher.cancel(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_TRUE(cancelled);
    EXPECT_FALSE(matcher.book().best_bid().has_value());

    matcher.stop();
}
