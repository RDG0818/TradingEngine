#include <gtest/gtest.h>
#include <vector>
#include "include/latent_price.h"
#include "include/traders/market_maker.h"
#include "include/traders/informed_trader.h"

TEST(MarketMaker, SubmitsBidAndAsk) {
    LatentPrice lp(640000000);
    MarketMaker mm(1, "mm1", 1000000000, lp, 20000);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    mm.tick(640000000, submit, cancel);

    ASSERT_EQ(orders.size(), 2u);

    auto get_side = [](const Order& o) {
        return std::visit([](const auto& x) { return x.side; }, o);
    };

    bool has_bid = get_side(orders[0]) == Side::Buy || get_side(orders[1]) == Side::Buy;
    bool has_ask = get_side(orders[0]) == Side::Sell || get_side(orders[1]) == Side::Sell;
    EXPECT_TRUE(has_bid);
    EXPECT_TRUE(has_ask);
}

TEST(MarketMaker, BidBelowAskBelowLatent) {
    LatentPrice lp(640000000);
    MarketMaker mm(1, "mm1", 1000000000, lp, 20000);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };
    mm.tick(640000000, submit, cancel);

    Price bid_price = 0, ask_price = 0;
    for (const auto& o : orders) {
        std::visit([&](const auto& x) {
            if constexpr (requires { x.price; }) {
                if (x.side == Side::Buy) bid_price = x.price;
                else ask_price = x.price;
            }
        }, o);
    }

    EXPECT_GT(bid_price, Price{0});
    EXPECT_GT(ask_price, Price{0});
    EXPECT_LT(bid_price, lp.get());
    EXPECT_GT(ask_price, lp.get());
    EXPECT_LT(bid_price, ask_price);
}

TEST(MarketMaker, CancelsPreviousOrdersOnNextTick) {
    LatentPrice lp(640000000);
    MarketMaker mm(1, "mm1", 1000000000, lp, 20000);

    std::vector<OrderId> cancelled;
    auto submit = [](Order) {};
    auto cancel = [&](OrderId id) -> bool {
        cancelled.push_back(id);
        return true;
    };

    mm.tick(640000000, submit, cancel);
    EXPECT_EQ(cancelled.size(), 0u);

    mm.tick(640000000, submit, cancel);
    EXPECT_EQ(cancelled.size(), 2u);
}

TEST(MarketMaker, WidensSpreadUnderAdverseSelection) {
    LatentPrice lp(640000000);
    MarketMaker mm(1, "mm1", 1000000000, lp, 20000);

    std::vector<Order> first_tick_orders;
    auto submit_first = [&](Order o) { first_tick_orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    mm.tick(640000000, submit_first, cancel);

    Fill f{};
    f.maker_trader_id = 1;
    f.fill_price = 640000000;
    f.fill_qty = 1;
    for (int i = 0; i < 20; i++) mm.on_fill(f);

    std::vector<Order> last_tick_orders;
    for (int i = 0; i < 20; i++) {
        last_tick_orders.clear();
        auto submit_last = [&](Order o) { last_tick_orders.push_back(o); };
        mm.tick(640000000, submit_last, cancel);
    }

    auto get_ask_price = [](const std::vector<Order>& orders) -> Price {
        for (const auto& o : orders) {
            Price p = 0;
            std::visit([&](const auto& x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (requires { x.price; }) {
                    if (x.side == Side::Sell) p = x.price;
                }
            }, o);
            if (p) return p;
        }
        return 0;
    };

    Price first_ask = get_ask_price(first_tick_orders);
    Price last_ask = get_ask_price(last_tick_orders);
    EXPECT_GT(last_ask - 640000000, first_ask - 640000000);
}

// ---------- InformedTrader tests ----------

TEST(InformedTrader, DoesNothingWhenSignalWeak) {
    // signal_noise=0 => signal == latent exactly
    // threshold=0.1 (10%) => need 10% divergence to trade
    // latent ~= last_price => no trade
    Price latent_price = 640000000;  // $64,000
    LatentPrice lp(latent_price);
    InformedTrader it(1, "it1", 1000000000, lp, /*signal_noise=*/0.0, /*threshold=*/0.1);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    // last_price == latent_price => signal / last_price = 1.0, well within 10% threshold
    it.tick(latent_price, submit, cancel);

    EXPECT_EQ(orders.size(), 0u);
}

TEST(InformedTrader, BuysWhenSignalAboveThreshold) {
    // signal_noise=0 => signal == latent exactly
    // latent = $64,000, last_price = $60,000 => signal/last ~ 1.067 > 1 + 0.002
    Price latent_price = 640000000;  // $64,000
    Price last_price   = 600000000;  // $60,000 — latent is >0.2% above last
    LatentPrice lp(latent_price);
    InformedTrader it(1, "it1", 1000000000, lp, /*signal_noise=*/0.0, /*threshold=*/0.002);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    it.tick(last_price, submit, cancel);

    ASSERT_GE(orders.size(), 1u);
    const auto& o = std::get<LimitOrder>(orders[0]);
    EXPECT_EQ(o.side, Side::Buy);
    EXPECT_EQ(o.tif, TimeInForce::IOC);
    EXPECT_EQ(o.price, latent_price);  // signal == latent when noise==0
}

TEST(InformedTrader, SellsWhenSignalBelowThreshold) {
    // signal_noise=0 => signal == latent exactly
    // latent = $60,000, last_price = $64,000 => signal/last ~ 0.9375 < 1 - 0.002
    Price latent_price = 600000000;  // $60,000
    Price last_price   = 640000000;  // $64,000 — latent is >0.2% below last
    LatentPrice lp(latent_price);
    InformedTrader it(1, "it1", 1000000000, lp, /*signal_noise=*/0.0, /*threshold=*/0.002);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    it.tick(last_price, submit, cancel);

    ASSERT_GE(orders.size(), 1u);
    const auto& o = std::get<LimitOrder>(orders[0]);
    EXPECT_EQ(o.side, Side::Sell);
    EXPECT_EQ(o.tif, TimeInForce::IOC);
    EXPECT_EQ(o.price, latent_price);  // signal == latent when noise==0
}

// ---------- NoiseTrader tests ----------
#include "include/traders/noise_trader.h"

TEST(NoiseTrader, SubmitsOrdersOnTick) {
    // lambda=1000 ensures near-certain order submission on a single tick
    NoiseTrader nt(50, "noise1", 1000000000, 1000.0);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    nt.tick(640000000, submit, cancel);

    EXPECT_GE(orders.size(), 1u);
}

TEST(NoiseTrader, OrdersHaveLimitAndMarketMix) {
    // lambda=1000 gives ~1000 orders per tick — guaranteed mix of both types
    NoiseTrader nt(51, "noise2", 1000000000, 1000.0);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    nt.tick(640000000, submit, cancel);

    bool has_limit  = false;
    bool has_market = false;
    for (const auto& o : orders) {
        std::visit([&](const auto& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, LimitOrder>)  has_limit  = true;
            if constexpr (std::is_same_v<T, MarketOrder>) has_market = true;
        }, o);
        if (has_limit && has_market) break;
    }
    EXPECT_TRUE(has_limit);
    EXPECT_TRUE(has_market);
}

TEST(NoiseTrader, ZeroLambdaSubmitsNothing) {
    NoiseTrader nt(52, "noise3", 1000000000, 0.0);

    std::vector<Order> orders;
    auto submit = [&](Order o) { orders.push_back(o); };
    auto cancel = [](OrderId) -> bool { return true; };

    for (int i = 0; i < 100; ++i) {
        nt.tick(640000000, submit, cancel);
    }
    EXPECT_EQ(orders.size(), 0u);
}
