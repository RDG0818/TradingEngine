// src/portfolio.cpp
#include "engine/portfolio.h"

Portfolio::Portfolio(uint64_t starting_balance)
    : starting_balance_(starting_balance), balance_(starting_balance) {}

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
    starting_balance_ = balance;
    balance_          = balance;
    position_         = 0;
    total_cost_       = 0;
    total_bought_     = 0;
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

int64_t Portfolio::realized_pnl() const {
    std::lock_guard lock(mutex_);
    Price avg = (total_bought_ > 0) ? (total_cost_ / total_bought_) : 0;
    // cash change since start + open position valued at avg cost (removes unrealized component)
    return static_cast<int64_t>(balance_) - static_cast<int64_t>(starting_balance_)
         + position_ * static_cast<int64_t>(avg);
}

int64_t Portfolio::total_pnl(Price current_price) const {
    std::lock_guard lock(mutex_);
    // balance change + mark-to-market value of open position
    return static_cast<int64_t>(balance_) - static_cast<int64_t>(starting_balance_)
         + position_ * static_cast<int64_t>(current_price);
}

Price Portfolio::avg_cost() const {
    std::lock_guard lock(mutex_);
    return (total_bought_ > 0) ? (total_cost_ / total_bought_) : 0;
}
