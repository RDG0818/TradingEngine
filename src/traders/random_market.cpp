// src/traders/random_market.cpp
#include "traders/random_market.h"
#include <chrono>

RandomMarketTrader::RandomMarketTrader(TraderId id, std::string name, uint64_t balance)
    : Trader(id, std::move(name), balance),
      rng_(std::random_device{}()) {}

void RandomMarketTrader::tick(Price /*last_price*/, SubmitFn submit) {
    int count = arrival_(rng_);
    for (int i = 0; i < count; ++i) {
        MarketOrder o;
        o.id        = next_id();
        o.trader_id = id_;
        o.side      = side_dist_(rng_) ? Side::Buy : Side::Sell;
        o.qty       = static_cast<Quantity>(qty_dist_(rng_));
        o.tif       = TimeInForce::IOC;
        o.ts        = std::chrono::steady_clock::now().time_since_epoch();
        ++orders_submitted_;
        submit(std::move(o));
    }
}
