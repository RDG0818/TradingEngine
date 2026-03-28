// include/traders/random_market.h
#pragma once
#include "trader.h"
#include <random>

class RandomMarketTrader : public Trader {
public:
    RandomMarketTrader(TraderId id, std::string name, uint64_t balance);
    void tick(Price last_price, SubmitFn submit) override;

private:
    std::mt19937                        rng_;
    std::poisson_distribution<int>      arrival_{1};   // avg 1 order per tick
    std::uniform_int_distribution<int>  qty_dist_{1, 10};
    std::bernoulli_distribution         side_dist_{0.5};
};
