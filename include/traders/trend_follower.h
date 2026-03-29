// include/traders/trend_follower.h
#pragma once
#include "trader.h"
#include <deque>
#include <optional>
#include <random>

// Dual moving-average crossover. Goes long when short MA > long MA, short otherwise.
class TrendFollowerTrader : public Trader {
public:
    TrendFollowerTrader(TraderId id, std::string name, uint64_t balance);
    void tick(Price last_price, SubmitFn submit) override;

private:
    std::deque<Price>                  history_;
    std::optional<Side>                current_position_;
    std::mt19937                       rng_;
    std::uniform_int_distribution<int> qty_dist_{10, 30};

    static constexpr size_t SHORT_WINDOW = 5;
    static constexpr size_t LONG_WINDOW  = 20;

    double moving_avg(size_t window) const;
};
