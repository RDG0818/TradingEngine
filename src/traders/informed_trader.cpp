// src/traders/informed_trader.cpp
#include "traders/informed_trader.h"
#include <cmath>
#include <algorithm>

InformedTrader::InformedTrader(TraderId id, std::string name, uint64_t balance,
                               const LatentPrice& latent,
                               double signal_noise, double threshold)
    : Trader(id, std::move(name), balance)
    , latent_(latent)
    , signal_noise_(signal_noise)
    , threshold_(threshold)
    , rng_(std::random_device{}())
    , noise_dist_(0.0, signal_noise)
    , qty_dist_(std::log(static_cast<double>(mean_qty_)), 0.5)
{}

void InformedTrader::on_fill(const Fill& fill) {
    Trader::on_fill(fill);
}

void InformedTrader::tick(Price last_price, SubmitFn submit, CancelFn /*cancel*/) {
    if (last_price == 0) return;

    Price latent_price = latent_.get();
    if (latent_price == 0) return;

    // Compute signal: latent_price * (1 + noise)
    // When signal_noise_ == 0, noise_dist_ has stddev 0 => always returns 0
    double noise = noise_dist_(rng_);
    double signal_double = static_cast<double>(latent_price) * (1.0 + noise);
    if (signal_double <= 0.0) return;
    Price signal = static_cast<Price>(signal_double);
    if (signal == 0) return;

    double last_d = static_cast<double>(last_price);
    double buy_threshold_price  = last_d * (1.0 + threshold_);
    double sell_threshold_price = last_d * (1.0 - threshold_);

    // Determine qty from log-normal, clamped to [1, max_qty_]
    auto sample_qty = [&]() -> uint64_t {
        double raw = qty_dist_(rng_);
        uint64_t q = static_cast<uint64_t>(std::round(raw));
        return std::clamp(q, uint64_t{1}, max_qty_);
    };

    if (static_cast<double>(signal) > buy_threshold_price) {
        uint64_t qty = sample_qty();
        OrderId oid = next_id();
        submit(LimitOrder{oid, id_, Side::Buy, signal, qty, TimeInForce::IOC, {}});
        orders_submitted_++;
    } else if (static_cast<double>(signal) < sell_threshold_price) {
        uint64_t qty = sample_qty();
        OrderId oid = next_id();
        submit(LimitOrder{oid, id_, Side::Sell, signal, qty, TimeInForce::IOC, {}});
        orders_submitted_++;
    }
    // else: signal not strong enough — do nothing
}
