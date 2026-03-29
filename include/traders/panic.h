// include/traders/panic.h
#pragma once
#include "trader.h"

// Spawned by market events. Dumps a fixed qty aggressively then goes dormant.
class PanicTrader : public Trader {
public:
    PanicTrader(TraderId id, std::string name, uint64_t balance,
                Side dump_side, Quantity dump_qty, int active_ticks);
    void tick(Price last_price, SubmitFn submit) override;
    bool is_done() const { return ticks_remaining_ <= 0; }

private:
    Side     dump_side_;
    Quantity dump_qty_;
    int      ticks_remaining_;
};
