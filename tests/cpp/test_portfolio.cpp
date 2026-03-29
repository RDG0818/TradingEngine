// tests/cpp/test_portfolio.cpp
#include <gtest/gtest.h>
#include "include/portfolio.h"

TEST(Portfolio, InitialState) {
    Portfolio p(1000000ULL); // $100.00 starting balance
    EXPECT_EQ(p.balance(), 1000000ULL);
    EXPECT_EQ(p.position(), 0LL);
}

TEST(Portfolio, BuyFillUpdatesBalanceAndPosition) {
    Portfolio p(1000000ULL);
    // Buy 10 units at price 50000 ($5.00 each = $50.00 total)
    p.apply_fill(Side::Buy, 50000ULL, 10);
    EXPECT_EQ(p.position(), 10LL);
    EXPECT_EQ(p.balance(), 1000000ULL - 50000ULL * 10);
}

TEST(Portfolio, SellFillUpdatesBalanceAndPosition) {
    Portfolio p(1000000ULL);
    p.apply_fill(Side::Buy,  50000ULL, 10);
    p.apply_fill(Side::Sell, 55000ULL, 5);
    EXPECT_EQ(p.position(), 5LL);
    EXPECT_EQ(p.balance(), 1000000ULL - 50000ULL * 10 + 55000ULL * 5);
}

TEST(Portfolio, UnrealizedPnL) {
    Portfolio p(1000000ULL);
    p.apply_fill(Side::Buy, 50000ULL, 10); // avg cost = 50000
    // At current price 55000: unrealized = (55000 - 50000) * 10 = 50000
    EXPECT_EQ(p.unrealized_pnl(55000ULL), 50000LL);
}

TEST(Portfolio, ResetRestoresBalance) {
    Portfolio p(1000000ULL);
    p.apply_fill(Side::Buy, 50000ULL, 10);
    p.reset(1000000ULL);
    EXPECT_EQ(p.balance(), 1000000ULL);
    EXPECT_EQ(p.position(), 0LL);
}
