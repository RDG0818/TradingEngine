// tests/cpp/test_order_book.cpp
#include <gtest/gtest.h>
#include "include/order_book.h"

static LimitOrder make_limit(OrderId id, TraderId tid, Side side, Price price, Quantity qty) {
    return {id, tid, side, price, qty, TimeInForce::GTC, {}};
}

TEST(OrderBook, BestBidAfterAdd) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Buy, 100, 50));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 100ULL);
}

TEST(OrderBook, BestAskAfterAdd) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Sell, 200, 30));
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 200ULL);
}

TEST(OrderBook, BestBidIsHighestPrice) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Buy, 100, 10));
    book.add_order(make_limit(2, 10, Side::Buy, 110, 10));
    book.add_order(make_limit(3, 10, Side::Buy, 90,  10));
    EXPECT_EQ(*book.best_bid(), 110ULL);
}

TEST(OrderBook, BestAskIsLowestPrice) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Sell, 200, 10));
    book.add_order(make_limit(2, 10, Side::Sell, 190, 10));
    book.add_order(make_limit(3, 10, Side::Sell, 210, 10));
    EXPECT_EQ(*book.best_ask(), 190ULL);
}

TEST(OrderBook, CancelRemovesOrder) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Buy, 100, 50));
    EXPECT_TRUE(book.cancel_order(1));
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBook, CancelNonexistentReturnsFalse) {
    OrderBook book;
    EXPECT_FALSE(book.cancel_order(999));
}

TEST(OrderBook, SnapshotReflectsBook) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Buy,  100, 50));
    book.add_order(make_limit(2, 10, Side::Buy,   90, 30));
    book.add_order(make_limit(3, 10, Side::Sell, 110, 20));
    auto snap = book.snapshot();
    ASSERT_EQ(snap.bids.size(), 2u);
    ASSERT_EQ(snap.asks.size(), 1u);
    EXPECT_EQ(snap.bids[0].first, 100ULL);  // best bid first
    EXPECT_EQ(snap.asks[0].first, 110ULL);
}

TEST(OrderBook, ForEachBidWalksDescending) {
    OrderBook book;
    book.add_order(make_limit(1, 10, Side::Buy, 100, 10));
    book.add_order(make_limit(2, 10, Side::Buy, 110, 10));
    book.add_order(make_limit(3, 10, Side::Buy,  90, 10));

    std::vector<Price> visited;
    book.for_each_bid([&](Price p, Quantity, const std::vector<OrderId>&) {
        visited.push_back(p);
        return false; // keep walking
    });
    ASSERT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], 110ULL);
    EXPECT_EQ(visited[1], 100ULL);
    EXPECT_EQ(visited[2],  90ULL);
}
