// src/traders/market_maker.cpp
#include "traders/market_maker.h"
#include <chrono>
#include <cmath>

MarketMakerTrader::MarketMakerTrader(TraderId id, std::string name,
                                     uint64_t balance, Price seed_price)
    : Trader(id, std::move(name), balance),
      ref_price_(seed_price),
      log_price_(std::log(static_cast<double>(seed_price))),
      rng_(std::random_device{}()) {}

void MarketMakerTrader::tick(Price /*last_price*/, SubmitFn submit) {
    // GBM step: log_price += noise
    log_price_ += gbm_noise_(rng_);
    ref_price_ = static_cast<Price>(std::exp(log_price_));

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    Quantity qty = static_cast<Quantity>(qty_dist_(rng_));

    LimitOrder bid;
    bid.id        = next_id();
    bid.trader_id = id_;
    bid.side      = Side::Buy;
    bid.price     = static_cast<Price>(ref_price_ - SPREAD_HALF);
    bid.qty       = qty;
    bid.tif       = TimeInForce::GTC;
    bid.ts        = now;

    LimitOrder ask;
    ask.id        = next_id();
    ask.trader_id = id_;
    ask.side      = Side::Sell;
    ask.price     = static_cast<Price>(ref_price_ + SPREAD_HALF);
    ask.qty       = qty;
    ask.tif       = TimeInForce::GTC;
    ask.ts        = now;

    orders_submitted_ += 2;
    submit(std::move(bid));
    submit(std::move(ask));
}
