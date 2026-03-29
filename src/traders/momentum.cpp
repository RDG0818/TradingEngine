// src/traders/momentum.cpp
#include "traders/momentum.h"
#include <chrono>
#include <numeric>

MomentumTrader::MomentumTrader(TraderId id, std::string name, uint64_t balance)
    : Trader(id, std::move(name), balance), rng_(std::random_device{}()) {}

void MomentumTrader::tick(Price last_price, SubmitFn submit) {
    price_history_.push_back(last_price);
    if (price_history_.size() > LOOKBACK) price_history_.pop_front();
    if (price_history_.size() < LOOKBACK) return;

    double oldest = static_cast<double>(price_history_.front());
    double newest = static_cast<double>(price_history_.back());
    double momentum = (newest - oldest) / oldest;

    if (std::abs(momentum) < THRESHOLD) return;

    Side side = (momentum > 0) ? Side::Buy : Side::Sell;
    double offset = (side == Side::Buy) ? 10.0 : -10.0; // cross spread slightly

    LimitOrder o;
    o.id        = next_id();
    o.trader_id = id_;
    o.side      = side;
    o.price     = static_cast<Price>(last_price + offset);
    o.qty       = static_cast<Quantity>(qty_dist_(rng_));
    o.tif       = TimeInForce::GTC;
    o.ts        = std::chrono::steady_clock::now().time_since_epoch();
    ++orders_submitted_;
    submit(std::move(o));
}
