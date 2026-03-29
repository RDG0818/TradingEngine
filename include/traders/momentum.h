// include/traders/momentum.h
#pragma once
#include "trader.h"
#include <deque>
#include <random>

class MomentumTrader : public Trader {
public:
    MomentumTrader(TraderId id, std::string name, uint64_t balance);
    void tick(Price last_price, SubmitFn submit) override;

private:
    std::deque<Price>                  price_history_;
    std::mt19937                       rng_;
    std::uniform_int_distribution<int> qty_dist_{5, 25};
    static constexpr size_t LOOKBACK = 10;
    static constexpr double THRESHOLD = 0.005; // 0.5% momentum signal
};
