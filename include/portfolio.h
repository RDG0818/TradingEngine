// include/portfolio.h
#pragma once
#include "order.h"
#include <mutex>

class Portfolio {
public:
    explicit Portfolio(uint64_t starting_balance);

    void apply_fill(Side side, Price fill_price, Quantity fill_qty);
    void reset(uint64_t balance);

    uint64_t balance()   const;
    int64_t  position()  const;  // positive = long, negative = short
    int64_t  unrealized_pnl(Price current_price) const;
    // realized = cash flow since start + value of open position at cost basis
    int64_t  realized_pnl() const;
    // total = realized + unrealized (equivalent to: cash change + position * current_price)
    int64_t  total_pnl(Price current_price) const;
    Price    avg_cost()  const;

private:
    mutable std::mutex mutex_;
    uint64_t starting_balance_;
    uint64_t balance_;
    int64_t  position_{0};
    uint64_t total_cost_{0};    // sum of (price * qty) for buys
    uint64_t total_bought_{0};  // total qty bought (for avg cost)
};
