#include "traders/market_maker.h"
#include <algorithm>
#include <cmath>

MarketMaker::MarketMaker(TraderId id, std::string name, uint64_t balance,
                         const LatentPrice& latent, Price half_spread)
    : Trader(id, std::move(name), balance)
    , latent_(latent)
    , half_spread_(half_spread)
{}

void MarketMaker::on_fill(const Fill& fill) {
    Trader::on_fill(fill);
    if (fill.maker_trader_id == id_) {
        fills_in_window_++;
    }
}

void MarketMaker::tick(Price /*last_price*/, SubmitFn submit, CancelFn cancel) {
    for (OrderId id : resting_bids_) cancel(id);
    for (OrderId id : resting_asks_) cancel(id);
    resting_bids_.clear();
    resting_asks_.clear();

    // Initialize effective_spread_ on first tick
    if (effective_spread_ == 0) effective_spread_ = half_spread_;

    ticks_in_window_++;
    if (ticks_in_window_ >= WINDOW_TICKS) {
        double fill_rate = static_cast<double>(fills_in_window_) / ticks_in_window_;
        if (fill_rate > 0.3) {
            double multiplier = std::min(1.0 + fill_rate * 2.0, MAX_SPREAD_MULTIPLIER);
            effective_spread_ = static_cast<Price>(half_spread_ * multiplier);
        } else {
            effective_spread_ = half_spread_;
        }
        fills_in_window_ = 0;
        ticks_in_window_ = 0;
    }
    Price effective_spread = effective_spread_;

    Price fair_value = latent_.get();
    if (fair_value == 0) return;

    Price bid_price = fair_value > effective_spread ? fair_value - effective_spread : 1;
    Price ask_price = fair_value + effective_spread;

    OrderId bid_id = next_id();
    submit(LimitOrder{bid_id, id_, Side::Buy, bid_price, 10, TimeInForce::GTC, {}});
    resting_bids_.push_back(bid_id);

    OrderId ask_id = next_id();
    submit(LimitOrder{ask_id, id_, Side::Sell, ask_price, 10, TimeInForce::GTC, {}});
    resting_asks_.push_back(ask_id);
}
