#pragma once
#include <random>
#include "trader.h"
#include "types.h"
#include "symbolRegistry.h"

class RandomTrader : public Trader {
public:
    RandomTrader(MatchingEngine& engine, EventDispatcher& dispatcher, const std::string& symbol, std::chrono::milliseconds interval);

    void tick() override;

private:
    SymbolID symbol_id_;
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point last_tick_;

    std::mt19937 rng_;
    std::uniform_int_distribution<int> side_dist_{0, 1};
    // Adjust price and quantity distributions as needed
    std::uniform_real_distribution<double> price_dist_{80.0, 120.0}; 
    std::uniform_int_distribution<int> quantity_dist_{1, 10};
};
