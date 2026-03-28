// src/traders/random_limit.cpp
#include "traders/random_limit.h"
#include <chrono>
#include <cmath>

RandomLimitTrader::RandomLimitTrader(TraderId id, std::string name, uint64_t balance)
    : Trader(id, std::move(name), balance),
      rng_(std::random_device{}()) {}

void RandomLimitTrader::tick(Price last_price, SubmitFn submit) {
    int count = arrival_(rng_);
    for (int i = 0; i < count; ++i) {
        bool is_buy = side_dist_(rng_);
        double raw_offset = offset_dist_(rng_);
        // Bids below last price, asks above — keep a spread.
        double offset = is_buy ? -std::abs(raw_offset) - 10.0
                                :  std::abs(raw_offset) + 10.0;
        Price price = static_cast<Price>(
            std::max(1.0, static_cast<double>(last_price) + offset));

        LimitOrder o;
        o.id        = next_id();
        o.trader_id = id_;
        o.side      = is_buy ? Side::Buy : Side::Sell;
        o.price     = price;
        o.qty       = static_cast<Quantity>(qty_dist_(rng_));
        o.tif       = TimeInForce::GTC;
        o.ts        = std::chrono::steady_clock::now().time_since_epoch();
        ++orders_submitted_;
        submit(std::move(o));
    }
}
