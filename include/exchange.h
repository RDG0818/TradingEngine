// include/exchange.h
#pragma once
#include "order.h"
#include "order_book.h"
#include "order_matcher.h"
#include "event_bus.h"
#include "trader_registry.h"
#include "portfolio.h"
#include "market_events.h"
#include "exchange_events.h"
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

struct SystemMetrics {
    uint64_t orders_processed{0};
    double   avg_latency_us{0.0};
    double   throughput_per_s{0.0};
    Price    last_trade_price{0};
};

struct PortfolioSnapshot {
    uint64_t balance;
    int64_t  position;
    int64_t  unrealized_pnl;
    Price    avg_cost;
};

using FillCallback       = std::function<void(const Fill&)>;
using BookUpdateCallback = std::function<void(const BookSnapshot&)>;

class Exchange {
public:
    Exchange();
    ~Exchange();

    void start(Price seed_price);
    void stop();
    bool is_running() const { return running_; }

    // User portfolio management.
    TraderId         create_portfolio(uint64_t balance);
    PortfolioSnapshot portfolio_snapshot(TraderId id) const;

    // Order operations.
    void submit_order(Order order);
    bool cancel_order(OrderId id);

    // Market data.
    BookSnapshot      book_snapshot() const;
    std::vector<Fill> recent_trades(size_t limit = 50) const;
    SystemMetrics     metrics() const;

    // Trader management (delegates to TraderRegistry).
    template<typename T, typename... Args>
    TraderId add_trader(std::string name, Args&&... args) {
        return registry_.add_trader<T>(std::move(name), std::forward<Args>(args)...);
    }
    void remove_trader(TraderId id)  { registry_.remove_trader(id); }
    void start_trader(TraderId id)   { registry_.start_trader(id); }
    void stop_trader(TraderId id)    { registry_.stop_trader(id); }
    void trigger_event(MarketEventType type, int duration_ticks = 30) {
        registry_.trigger_event(type, duration_ticks);
    }
    std::vector<TraderInfo> all_traders() const { return registry_.all_traders(); }

    // Python callback hooks.
    void on_fill_callback(FillCallback cb);
    void on_book_update_callback(BookUpdateCallback cb);

    EventBus& event_bus() { return bus_; }

private:
    void on_fill(const FillEvent& e);

    EventBus       bus_;
    OrderMatcher   matcher_;
    TraderRegistry registry_;

    mutable std::mutex                       portfolios_mutex_;
    std::unordered_map<TraderId, Portfolio>  portfolios_;
    std::atomic<TraderId>                    next_portfolio_id_{1};

    mutable std::mutex trades_mutex_;
    std::deque<Fill>   recent_trades_;
    static constexpr size_t MAX_TRADE_HISTORY = 500;

    std::atomic<bool>     running_{false};
    std::atomic<uint64_t> orders_processed_{0};
    Price                 last_trade_price_{0};
    Price                 seed_price_{0};

    FillCallback       fill_cb_;
    BookUpdateCallback book_update_cb_;
    std::mutex         cb_mutex_;
};
