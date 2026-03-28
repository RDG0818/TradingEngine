// include/traders/market_maker.h
#pragma once
#include "trader.h"
#include <random>

class MarketMakerTrader : public Trader {
public:
    // seed_price: initial reference price for GBM (in Price units).
    MarketMakerTrader(TraderId id, std::string name, uint64_t balance, Price seed_price);
    void tick(Price last_price, SubmitFn submit) override;

private:
    Price                              ref_price_;
    double                             log_price_;   // log of ref_price for GBM
    std::mt19937                       rng_;
    std::normal_distribution<double>   gbm_noise_{0.0, 0.001}; // sigma per tick
    std::uniform_int_distribution<int> qty_dist_{5, 20};

    static constexpr double SPREAD_HALF = 50.0; // half-spread in Price units ($0.005)
};
