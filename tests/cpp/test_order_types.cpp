#include <gtest/gtest.h>
#include "include/order.h"

TEST(OrderTypes, VariantHoldsLimitOrder) {
    LimitOrder lo{1, 10, Side::Buy, 642000000ULL, 100, TimeInForce::GTC, {}};
    Order o = lo;
    EXPECT_EQ(get_order_id(o), 1ULL);
    EXPECT_EQ(get_trader_id(o), 10ULL);
    EXPECT_EQ(get_side(o), Side::Buy);
    EXPECT_EQ(get_qty(o), 100ULL);
}

TEST(OrderTypes, VariantHoldsMarketOrder) {
    MarketOrder mo{2, 11, Side::Sell, 50, TimeInForce::IOC, {}};
    Order o = mo;
    EXPECT_EQ(get_order_id(o), 2ULL);
    EXPECT_EQ(get_side(o), Side::Sell);
}

TEST(OrderTypes, VariantHoldsStopLimitOrder) {
    StopLimitOrder slo{3, 12, Side::Sell, 640000000ULL, 639000000ULL, 200, {}};
    Order o = slo;
    EXPECT_EQ(get_order_id(o), 3ULL);
}

TEST(OrderTypes, FillStruct) {
    Fill f{1, 2, 10, 11, 642000000ULL, 50, {}};
    EXPECT_EQ(f.maker_order_id, 1ULL);
    EXPECT_EQ(f.fill_qty, 50ULL);
}
