// src/traders/trend_follower.cpp
#include "traders/trend_follower.h"
#include <chrono>
#include <numeric>

TrendFollowerTrader::TrendFollowerTrader(TraderId id, std::string name, uint64_t balance)
    : Trader(id, std::move(name), balance), rng_(std::random_device{}()) {}

double TrendFollowerTrader::moving_avg(size_t window) const {
    auto begin = history_.end() - static_cast<ptrdiff_t>(window);
    double sum = 0;
    for (auto it = begin; it != history_.end(); ++it) sum += static_cast<double>(*it);
    return sum / static_cast<double>(window);
}

void TrendFollowerTrader::tick(Price last_price, SubmitFn submit) {
    history_.push_back(last_price);
    if (history_.size() > LONG_WINDOW + 5) history_.pop_front();
    if (history_.size() < LONG_WINDOW) return;

    double short_ma = moving_avg(SHORT_WINDOW);
    double long_ma  = moving_avg(LONG_WINDOW);
    Side signal     = (short_ma > long_ma) ? Side::Buy : Side::Sell;

    if (current_position_ == signal) return; // already aligned
    current_position_ = signal;

    // Flip position: aggressive limit order to cross the spread.
    double offset = (signal == Side::Buy) ? 15.0 : -15.0;
    LimitOrder o;
    o.id        = next_id();
    o.trader_id = id_;
    o.side      = signal;
    o.price     = static_cast<Price>(last_price + offset);
    o.qty       = static_cast<Quantity>(qty_dist_(rng_));
    o.tif       = TimeInForce::GTC;
    o.ts        = std::chrono::steady_clock::now().time_since_epoch();
    ++orders_submitted_;
    submit(std::move(o));
}
