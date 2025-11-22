#pragma once
#include "trader.h"
#include "eventDispatcher.h"
#include <deque>
#include <numeric>

class TrendFollowingTrader : public Trader, public EventListener {
public:
    TrendFollowingTrader(MatchingEngine& engine, EventDispatcher& dispatcher, const std::string& symbol, std::chrono::milliseconds interval, size_t trend_window);

    void onEvent(const Event& event) override;
    void tick() override;

private:
    SymbolID symbol_id_;
    std::chrono::milliseconds interval_;
    size_t trend_window_;
    std::deque<Price> price_history_;
    std::mutex mtx_;
    std::chrono::steady_clock::time_point last_tick_;
};
