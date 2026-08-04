#include "engine/order_book.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

void OrderBook::add_order(const LimitOrder& order) {
    std::unique_lock lock(mutex_);
    auto& side = (order.side == Side::Buy) ? bids_ : asks_;
    auto& level = side[order.price];
    level.total_qty += order.qty;
    level.order_ids.push_back(order.id);
    orders_.emplace(order.id, order);
}

bool OrderBook::cancel_order(OrderId id) {
    std::unique_lock lock(mutex_);
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;
    const LimitOrder& order = it->second;
    auto& side = (order.side == Side::Buy) ? bids_ : asks_;
    auto level_it = side.find(order.price);
    if (level_it != side.end()) {
        level_it->second.total_qty -= order.qty;
        level_it->second.order_ids.remove(id);
        if (level_it->second.order_ids.empty())
            side.erase(level_it);
    }
    orders_.erase(it);
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    std::shared_lock lock(mutex_);
    if (bids_.empty()) return std::nullopt;
    return bids_.rbegin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    std::shared_lock lock(mutex_);
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

const LimitOrder* OrderBook::find_order(OrderId id) const {
    std::shared_lock lock(mutex_);
    auto it = orders_.find(id);
    return (it != orders_.end()) ? &it->second : nullptr;
}

void OrderBook::for_each_bid(BookWalkCallback cb) {
    // Snapshot price levels under lock, then walk without lock.
    struct LevelCopy { Price price; Quantity qty; std::vector<OrderId> ids; };
    std::vector<LevelCopy> levels;
    {
        std::shared_lock lock(mutex_);
        for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
            std::vector<OrderId> ids(it->second.order_ids.begin(), it->second.order_ids.end());
            levels.push_back({it->first, it->second.total_qty, std::move(ids)});
        }
    }
    for (const auto& lvl : levels)
        if (cb(lvl.price, lvl.qty, lvl.ids)) break;
}

void OrderBook::for_each_ask(BookWalkCallback cb) {
    struct LevelCopy { Price price; Quantity qty; std::vector<OrderId> ids; };
    std::vector<LevelCopy> levels;
    {
        std::shared_lock lock(mutex_);
        for (const auto& [price, level] : asks_) {
            std::vector<OrderId> ids(level.order_ids.begin(), level.order_ids.end());
            levels.push_back({price, level.total_qty, ids});
        }
    }
    for (const auto& lvl : levels)
        if (cb(lvl.price, lvl.qty, lvl.ids)) break;
}

BookSnapshot OrderBook::snapshot() const {
    std::shared_lock lock(mutex_);
    BookSnapshot snap;
    for (auto it = bids_.rbegin(); it != bids_.rend(); ++it)
        snap.bids.emplace_back(it->first, it->second.total_qty);
    for (const auto& [price, level] : asks_)
        snap.asks.emplace_back(price, level.total_qty);
    return snap;
}
