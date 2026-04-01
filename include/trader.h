// include/trader.h
#pragma once
#include "order.h"
#include "portfolio.h"
#include <functional>
#include <string>
#include <atomic>

// Callback the trader uses to submit orders.
using SubmitFn = std::function<void(Order)>;
// Callback the trader uses to cancel a resting order by ID.
using CancelFn = std::function<bool(OrderId)>;

struct TraderMetrics {
    double  orders_per_second{0.0};
    int64_t pnl{0};              // realized PnL in price units
    int64_t position{0};
};

class Trader {
public:
    explicit Trader(TraderId id, std::string name, uint64_t starting_balance);
    virtual ~Trader() = default;

    // Called each tick by TraderRegistry.
    virtual void tick(Price last_price, SubmitFn submit, CancelFn cancel) = 0;

    // Called when a fill is received for one of this trader's orders.
    virtual void on_fill(const Fill& fill) {
        if (fill.maker_trader_id != id_ && fill.taker_trader_id != id_) return;
        bool is_taker = (fill.taker_trader_id == id_);
        Side side = is_taker ? fill.taker_side
                             : (fill.taker_side == Side::Buy ? Side::Sell : Side::Buy);
        portfolio_.apply_fill(side, fill.fill_price, fill.fill_qty);
    }

    void reset() { portfolio_.reset(starting_balance_); }

    TraderId           id()      const { return id_; }
    const std::string& name()    const { return name_; }
    TraderMetrics      metrics() const;

    // Public order ID allocator for external use (e.g. TUI user orders)
    static OrderId alloc_order_id() {
        return next_order_id_.fetch_add(1, std::memory_order_relaxed);
    }

protected:
    TraderId    id_;
    std::string name_;
    uint64_t    starting_balance_;
    Portfolio   portfolio_;
    std::atomic<uint64_t> orders_submitted_{0};

    static std::atomic<OrderId> next_order_id_;
    OrderId next_id() { return next_order_id_.fetch_add(1, std::memory_order_relaxed); }
};
