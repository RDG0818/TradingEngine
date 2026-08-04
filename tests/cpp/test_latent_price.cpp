#include <gtest/gtest.h>
#include "include/market/latent_price.h"

TEST(LatentPrice, InitializesToSeedPrice) {
    LatentPrice lp(640000000);  // $64,000.00
    EXPECT_EQ(lp.get(), 640000000ULL);
}

TEST(LatentPrice, TickChangesPrice) {
    LatentPrice lp(640000000, 0.01);  // high sigma so it moves
    Price before = lp.get();
    for (int i = 0; i < 100; i++) lp.tick();
    Price after = lp.get();
    EXPECT_NE(before, after);
}

TEST(LatentPrice, PriceRemainsPositive) {
    LatentPrice lp(100000, 0.1);  // $10 seed, very high sigma
    for (int i = 0; i < 10000; i++) lp.tick();
    EXPECT_GT(lp.get(), Price{0});
}

TEST(LatentPrice, SetSigmaChangesVolatility) {
    LatentPrice lp(640000000, 0.0);  // zero vol
    for (int i = 0; i < 100; i++) lp.tick();
    EXPECT_EQ(lp.get(), 640000000ULL);  // no movement with zero sigma
    lp.set_sigma(0.01);
    for (int i = 0; i < 100; i++) lp.tick();
    EXPECT_DOUBLE_EQ(lp.sigma(), 0.01);
}
