// src/portfolio.cpp
#include "portfolio.h"

Portfolio::Portfolio(uint64_t starting_balance) : balance_(starting_balance) {}

void Portfolio::apply_fill(Side side, Price fill_price, Quantity fill_qty) {
    std::lock_guard lock(mutex_);
    if (side == Side::Buy) {
        balance_       -= fill_price * fill_qty;
        position_      += static_cast<int64_t>(fill_qty);
        total_cost_    += fill_price * fill_qty;
        total_bought_  += fill_qty;
    } else {
        balance_       += fill_price * fill_qty;
        position_      -= static_cast<int64_t>(fill_qty);
    }
}

void Portfolio::reset(uint64_t balance) {
    std::lock_guard lock(mutex_);
    balance_      = balance;
    position_     = 0;
    total_cost_   = 0;
    total_bought_ = 0;
}

uint64_t Portfolio::balance() const {
    std::lock_guard lock(mutex_);
    return balance_;
}

int64_t Portfolio::position() const {
    std::lock_guard lock(mutex_);
    return position_;
}

int64_t Portfolio::unrealized_pnl(Price current_price) const {
    std::lock_guard lock(mutex_);
    if (position_ == 0) return 0;
    Price avg = (total_bought_ > 0) ? (total_cost_ / total_bought_) : 0;
    return position_ * static_cast<int64_t>(current_price - avg);
}

Price Portfolio::avg_cost() const {
    std::lock_guard lock(mutex_);
    return (total_bought_ > 0) ? (total_cost_ / total_bought_) : 0;
}
