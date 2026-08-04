#include <gtest/gtest.h>
#include "include/tui/order_command_parser.h"

TEST(OrderCommandParser, LimitBuy) {
    std::string error;
    auto result = parse_order_command("buy 5 @ 64000", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<LimitOrder>(result->order));
    auto lo = std::get<LimitOrder>(result->order);
    EXPECT_EQ(lo.side, Side::Buy);
    EXPECT_EQ(lo.qty, 5ULL);
    EXPECT_EQ(lo.price, 640000000ULL);
    EXPECT_EQ(lo.tif, TimeInForce::GTC);
    EXPECT_TRUE(result->tracks_resting_price);
}

TEST(OrderCommandParser, LimitSellFok) {
    std::string error;
    auto result = parse_order_command("sell 3 @ 100 fok", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<LimitOrder>(result->order));
    auto lo = std::get<LimitOrder>(result->order);
    EXPECT_EQ(lo.tif, TimeInForce::FOK);
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, MarketOrder) {
    std::string error;
    auto result = parse_order_command("buy 10", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<MarketOrder>(result->order));
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, StopMarket) {
    std::string error;
    auto result = parse_order_command("sell 2 stop 63000", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<StopMarketOrder>(result->order));
    auto so = std::get<StopMarketOrder>(result->order);
    EXPECT_EQ(so.stop_price, 630000000ULL);
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, StopLimit) {
    std::string error;
    auto result = parse_order_command("buy 2 stop 63000 @ 63100", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<StopLimitOrder>(result->order));
    auto so = std::get<StopLimitOrder>(result->order);
    EXPECT_EQ(so.stop_price, 630000000ULL);
    EXPECT_EQ(so.limit_price, 631000000ULL);
}

TEST(OrderCommandParser, RejectsUnknownVerb) {
    std::string error;
    auto result = parse_order_command("frobnicate 5", 1, error);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(error.empty());
}

TEST(OrderCommandParser, RejectsBadQty) {
    std::string error;
    auto result = parse_order_command("buy -5 @ 100", 1, error);
    EXPECT_FALSE(result.has_value());
}

TEST(OrderCommandParser, RejectsBadStopSyntax) {
    std::string error;
    auto result = parse_order_command("buy 5 stop", 1, error);
    EXPECT_FALSE(result.has_value());
}
