// include/traders/twap.h
#pragma once
#include "trader.h"

// Splits a large directional order into equal-sized slices over N ticks.
class TWAPTrader : public Trader {
public:
    // total_qty: total order size; slices: number of equal parts; side: Buy or Sell.
    TWAPTrader(TraderId id, std::string name, uint64_t balance,
               Quantity total_qty, int slices, Side side);
    void tick(Price last_price, SubmitFn submit) override;

private:
    Quantity remaining_;
    Quantity slice_qty_;
    Side     target_side_;
    int      ticks_elapsed_{0};
    int      total_slices_;
};
