// src/traders/panic.cpp
#include "traders/panic.h"
#include <chrono>

PanicTrader::PanicTrader(TraderId id, std::string name, uint64_t balance,
                         Side dump_side, Quantity dump_qty, int active_ticks)
    : Trader(id, std::move(name), balance),
      dump_side_(dump_side), dump_qty_(dump_qty), ticks_remaining_(active_ticks) {}

void PanicTrader::tick(Price /*last_price*/, SubmitFn submit) {
    if (ticks_remaining_ <= 0) return;
    --ticks_remaining_;

    MarketOrder o;
    o.id        = next_id();
    o.trader_id = id_;
    o.side      = dump_side_;
    o.qty       = dump_qty_;
    o.tif       = TimeInForce::IOC;
    o.ts        = std::chrono::steady_clock::now().time_since_epoch();
    ++orders_submitted_;
    submit(std::move(o));
}
