// src/traders/twap.cpp
#include "traders/twap.h"
#include <chrono>

TWAPTrader::TWAPTrader(TraderId id, std::string name, uint64_t balance,
                       Quantity total_qty, int slices, Side side)
    : Trader(id, std::move(name), balance),
      remaining_(total_qty),
      slice_qty_(total_qty / static_cast<Quantity>(slices)),
      target_side_(side),
      total_slices_(slices) {}

void TWAPTrader::tick(Price /*last_price*/, SubmitFn submit) {
    if (remaining_ == 0) return;
    ++ticks_elapsed_;

    Quantity qty = std::min(slice_qty_, remaining_);
    remaining_ -= qty;

    // Market order to guarantee execution at the scheduled time.
    MarketOrder o;
    o.id        = next_id();
    o.trader_id = id_;
    o.side      = target_side_;
    o.qty       = qty;
    o.tif       = TimeInForce::IOC;
    o.ts        = std::chrono::steady_clock::now().time_since_epoch();
    ++orders_submitted_;
    submit(std::move(o));
}
