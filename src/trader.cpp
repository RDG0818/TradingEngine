// src/trader.cpp
#include "trader.h"

std::atomic<OrderId> Trader::next_order_id_{100000};

Trader::Trader(TraderId id, std::string name, uint64_t starting_balance)
    : id_(id), name_(std::move(name)),
      starting_balance_(starting_balance),
      portfolio_(starting_balance) {}

TraderMetrics Trader::metrics() const {
    return TraderMetrics{
        .orders_per_second = static_cast<double>(orders_submitted_.load()),
        .pnl      = portfolio_.unrealized_pnl(0),
        .position = portfolio_.position()
    };
}
