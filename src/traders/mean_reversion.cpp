// src/traders/mean_reversion.cpp
#include "traders/mean_reversion.h"
#include <chrono>
#include <numeric>

MeanReversionTrader::MeanReversionTrader(TraderId id, std::string name, uint64_t balance)
    : Trader(id, std::move(name), balance), rng_(std::random_device{}()) {}

void MeanReversionTrader::tick(Price last_price, SubmitFn submit) {
    price_history_.push_back(last_price);
    if (price_history_.size() > LOOKBACK) price_history_.pop_front();
    if (price_history_.size() < LOOKBACK) return;

    double sum  = 0;
    for (Price p : price_history_) sum += static_cast<double>(p);
    double mean = sum / static_cast<double>(price_history_.size());
    double dev  = (static_cast<double>(last_price) - mean) / mean;

    if (std::abs(dev) < THRESHOLD) return;

    // Price above mean → sell (expect reversion down); below → buy.
    Side side = (dev > 0) ? Side::Sell : Side::Buy;
    double offset = (side == Side::Buy) ? 5.0 : -5.0;

    LimitOrder o;
    o.id        = next_id();
    o.trader_id = id_;
    o.side      = side;
    o.price     = static_cast<Price>(mean + offset);
    o.qty       = static_cast<Quantity>(qty_dist_(rng_));
    o.tif       = TimeInForce::GTC;
    o.ts        = std::chrono::steady_clock::now().time_since_epoch();
    ++orders_submitted_;
    submit(std::move(o));
}
