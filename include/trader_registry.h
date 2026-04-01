// include/trader_registry.h
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "event_bus.h"
#include "latent_price.h"
#include "order_matcher.h"
#include "trader.h"
#include "traders/market_maker.h"
#include "traders/informed_trader.h"
#include "traders/noise_trader.h"

class TraderRegistry {
public:
    TraderRegistry(OrderMatcher& matcher, EventBus& bus,
                   Price seed_price, double sigma = 0.0003);
    ~TraderRegistry();

    void start();
    void stop();

    TraderId add_market_maker(std::string name, uint64_t balance);
    TraderId add_informed_trader(std::string name, uint64_t balance);
    TraderId add_noise_trader(std::string name, uint64_t balance, double lambda = 0.7);

    void remove_trader(TraderId id);

    void pause_all();
    void resume_all();

    void set_market_maker_count(size_t n, uint64_t balance = 1000000000);
    void set_informed_count(size_t n, uint64_t balance = 1000000000);
    void set_noise_count(size_t n, uint64_t balance = 1000000000);

    void set_sigma(double sigma) { latent_.set_sigma(sigma); }
    void set_market_maker_spread(Price half_spread);
    void set_tick_interval_ms(int ms);

    void reset_latent(Price seed_price) { latent_.reinit(seed_price, latent_.sigma()); }

    LatentPrice& latent_price() { return latent_; }
    const LatentPrice& latent_price() const { return latent_; }

private:
    void tick_loop();
    void subscribe_to_fills();

    OrderMatcher& matcher_;
    EventBus& bus_;
    LatentPrice latent_;

    struct TraderEntry {
        std::unique_ptr<Trader> trader;
        bool active;
        std::string type;
    };

    std::unordered_map<TraderId, TraderEntry> traders_;
    std::vector<SubscriptionToken> fill_tokens_;

    std::thread tick_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<int> tick_interval_ms_{200};

    mutable std::mutex mutex_;
    std::atomic<TraderId> next_id_{2000};
};
