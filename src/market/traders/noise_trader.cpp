#include "market/traders/noise_trader.h"
#include <algorithm>
#include <cmath>
#include <chrono>

NoiseTrader::NoiseTrader(TraderId id, std::string name, uint64_t balance, double lambda)
    : Trader(id, std::move(name), balance)
    , lambda_(lambda)
    , price_noise_(5000)
    , rng_(std::random_device{}())
    , arrival_(lambda)
    , size_dist_(std::log(MEAN_QTY) - 0.5 * QTY_SIGMA * QTY_SIGMA, QTY_SIGMA)
    , limit_dist_(LIMIT_PROB)
    , side_dist_(0.5)
    , offset_dist_(0.0, 1.0)  // unit normal; scaled by price_noise_ at call site
{}

void NoiseTrader::on_fill(const Fill& fill) {
    Trader::on_fill(fill);
}

void NoiseTrader::tick(Price last_price, SubmitFn submit, CancelFn /*cancel*/) {
    if (last_price == 0) return;

    int n = arrival_(rng_);
    for (int i = 0; i < n; ++i) {
        Side side = side_dist_(rng_) ? Side::Buy : Side::Sell;

        double raw_qty = size_dist_(rng_);
        uint64_t qty = static_cast<uint64_t>(std::clamp(raw_qty, 1.0, static_cast<double>(MAX_QTY)));
        if (qty == 0) qty = 1;

        auto ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch());

        if (limit_dist_(rng_)) {
            double offset = offset_dist_(rng_) * static_cast<double>(price_noise_);
            double raw_price = static_cast<double>(last_price) + offset;
            Price price;
            if (raw_price < 1.0)
                price = 1;
            else if (raw_price > static_cast<double>(PRICE_MAX))
                price = PRICE_MAX;
            else
                price = static_cast<Price>(raw_price);

            submit(LimitOrder{next_id(), id_, side, price, qty, TimeInForce::GTC, ts});
        } else {
            submit(MarketOrder{next_id(), id_, side, qty, TimeInForce::GTC, ts});
        }
        orders_submitted_++;
    }
}
