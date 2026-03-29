// include/traders/random_limit.h
#pragma once
#include "trader.h"
#include <random>

class RandomLimitTrader : public Trader {
public:
    RandomLimitTrader(TraderId id, std::string name, uint64_t balance);
    void tick(Price last_price, SubmitFn submit) override;

private:
    std::mt19937                        rng_;
    std::poisson_distribution<int>      arrival_{1};
    std::normal_distribution<double>    offset_dist_{0.0, 20.0}; // price offset in units
    std::uniform_int_distribution<int>  qty_dist_{1, 20};
    std::bernoulli_distribution         side_dist_{0.5};
};
