// include/exchange.h
#pragma once
#include "order.h"
#include "order_book.h"
#include "order_matcher.h"
#include "event_bus.h"
#include "trader_registry.h"
#include "portfolio.h"
#include "exchange_events.h"
#include <atomic>
#include <deque>
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
    uint64_t balance{0};
    int64_t  position{0};
    int64_t  unrealized_pnl{0};
    int64_t  realized_pnl{0};
    int64_t  total_pnl{0};
    Price    avg_cost{0};
};

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
    TraderId add_market_maker(std::string name, uint64_t balance)  { return registry_.add_market_maker(std::move(name), balance); }
    TraderId add_informed_trader(std::string name, uint64_t balance) { return registry_.add_informed_trader(std::move(name), balance); }
    TraderId add_noise_trader(std::string name, uint64_t balance, double lambda = 0.7) { return registry_.add_noise_trader(std::move(name), balance, lambda); }
    void remove_trader(TraderId id) { registry_.remove_trader(id); }
    void pause_traders()  { registry_.pause_all(); }
    void resume_traders() { registry_.resume_all(); }
    TraderRegistry& registry() { return registry_; }

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
};
