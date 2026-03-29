// include/trader_registry.h
#pragma once
#include "trader.h"
#include "order_matcher.h"
#include "traders/random_market.h"
#include "traders/random_limit.h"
#include "traders/market_maker.h"
#include "traders/momentum.h"
#include "traders/mean_reversion.h"
#include "traders/twap.h"
#include "traders/trend_follower.h"
#include "traders/panic.h"
#include "market_events.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct TraderInfo {
    TraderId    id;
    std::string name;
    std::string type;
    bool        active;
    TraderMetrics metrics;
};

class TraderRegistry {
public:
    explicit TraderRegistry(OrderMatcher& matcher);
    ~TraderRegistry();

    void start();  // begin tick loop
    void stop();

    // Create a trader and return its id. Not started until start_trader() is called.
    template<typename T, typename... Args>
    TraderId add_trader(std::string name, Args&&... args) {
        TraderId id = next_trader_id_.fetch_add(1);
        std::unique_lock lock(mutex_);
        traders_.emplace(id, std::make_unique<T>(id, std::move(name), std::forward<Args>(args)...));
        active_traders_.emplace(id, false);
        type_names_.emplace(id, typeid(T).name());
        return id;
    }

    void remove_trader(TraderId id);
    void start_trader(TraderId id);
    void stop_trader(TraderId id);

    void trigger_event(MarketEventType type, int duration_ticks = 30);

    std::optional<TraderInfo>  trader_info(TraderId id) const;
    std::vector<TraderInfo>    all_traders() const;

    // Subscribe to fills so traders can update their portfolios.
    void subscribe_to_fills(EventBus& bus);

private:
    void tick_loop();
    Price last_price_{0};

    OrderMatcher&                                         matcher_;
    std::unordered_map<TraderId, std::unique_ptr<Trader>> traders_;
    std::unordered_map<TraderId, bool>                    active_traders_;
    std::unordered_map<TraderId, std::string>             type_names_;
    mutable std::mutex                                    mutex_;
    std::atomic<bool>                                     running_{false};
    std::thread                                           tick_thread_;
    std::atomic<TraderId>                                 next_trader_id_{1000};

    // Track panic traders spawned by events for cleanup.
    std::vector<TraderId> event_traders_;
};
