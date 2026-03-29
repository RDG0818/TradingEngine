// include/traders/mean_reversion.h
#pragma once
#include "trader.h"
#include <deque>
#include <random>

class MeanReversionTrader : public Trader {
public:
    MeanReversionTrader(TraderId id, std::string name, uint64_t balance);
    void tick(Price last_price, SubmitFn submit) override;

private:
    std::deque<Price>                  price_history_;
    std::mt19937                       rng_;
    std::uniform_int_distribution<int> qty_dist_{5, 20};
    static constexpr size_t LOOKBACK   = 20;
    static constexpr double THRESHOLD  = 0.02; // 2% deviation from mean triggers trade
};
