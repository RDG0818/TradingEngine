// src/trader_registry.cpp
#include "trader_registry.h"
#include "exchange_events.h"

TraderRegistry::TraderRegistry(OrderMatcher& matcher, EventBus& bus,
                               Price seed_price, double sigma)
    : matcher_(matcher)
    , bus_(bus)
    , latent_(seed_price, sigma)
{}

TraderRegistry::~TraderRegistry() {
    stop();
    for (auto token : fill_tokens_) bus_.unsubscribe(token);
}

void TraderRegistry::start() {
    subscribe_to_fills();
    running_.store(true);
    tick_thread_ = std::thread(&TraderRegistry::tick_loop, this);
}

void TraderRegistry::stop() {
    running_.store(false);
    if (tick_thread_.joinable()) tick_thread_.join();
}

void TraderRegistry::subscribe_to_fills() {
    fill_tokens_.push_back(bus_.subscribe<FillEvent>([this](const FillEvent& e) {
        std::unique_lock lock(mutex_);
        for (auto& [id, entry] : traders_) {
            entry.trader->on_fill(e.fill);
        }
    }));
}

void TraderRegistry::tick_loop() {
    while (running_.load()) {
        latent_.tick();

        if (!paused_.load()) {
            Price last_price = latent_.get();

            auto submit_fn = [this](Order order) {
                matcher_.submit(std::move(order));
            };
            auto cancel_fn = [this](OrderId id) -> bool {
                matcher_.cancel(id);
                return true;
            };

            std::unique_lock lock(mutex_);
            for (auto& [id, entry] : traders_) {
                if (entry.active) {
                    entry.trader->tick(last_price, submit_fn, cancel_fn);
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(tick_interval_ms_.load()));
    }
}

TraderId TraderRegistry::add_market_maker(std::string name, uint64_t balance) {
    TraderId id = next_id_.fetch_add(1);
    std::unique_lock lock(mutex_);
    auto mm = std::make_unique<MarketMaker>(id, std::move(name), balance, latent_);
    traders_.emplace(id, TraderEntry{std::move(mm), true, "mm"});
    return id;
}

TraderId TraderRegistry::add_informed_trader(std::string name, uint64_t balance) {
    TraderId id = next_id_.fetch_add(1);
    std::unique_lock lock(mutex_);
    auto it = std::make_unique<InformedTrader>(id, std::move(name), balance, latent_);
    traders_.emplace(id, TraderEntry{std::move(it), true, "informed"});
    return id;
}

TraderId TraderRegistry::add_noise_trader(std::string name, uint64_t balance, double lambda) {
    TraderId id = next_id_.fetch_add(1);
    std::unique_lock lock(mutex_);
    auto nt = std::make_unique<NoiseTrader>(id, std::move(name), balance, lambda);
    traders_.emplace(id, TraderEntry{std::move(nt), true, "noise"});
    return id;
}

void TraderRegistry::remove_trader(TraderId id) {
    std::unique_lock lock(mutex_);
    traders_.erase(id);
}

void TraderRegistry::pause_all() {
    paused_.store(true);
}

void TraderRegistry::resume_all() {
    paused_.store(false);
}

void TraderRegistry::set_market_maker_count(size_t n, uint64_t balance) {
    std::vector<TraderId> mm_ids;
    {
        std::unique_lock lock(mutex_);
        for (auto& [id, entry] : traders_) {
            if (entry.type == "mm") mm_ids.push_back(id);
        }
    }
    while (mm_ids.size() > n) {
        remove_trader(mm_ids.back());
        mm_ids.pop_back();
    }
    while (mm_ids.size() < n) {
        mm_ids.push_back(add_market_maker("mm" + std::to_string(next_id_.load()), balance));
    }
}

void TraderRegistry::set_informed_count(size_t n, uint64_t balance) {
    std::vector<TraderId> ids;
    {
        std::unique_lock lock(mutex_);
        for (auto& [id, entry] : traders_) {
            if (entry.type == "informed") ids.push_back(id);
        }
    }
    while (ids.size() > n) {
        remove_trader(ids.back());
        ids.pop_back();
    }
    while (ids.size() < n) {
        ids.push_back(add_informed_trader("inf" + std::to_string(next_id_.load()), balance));
    }
}

void TraderRegistry::set_noise_count(size_t n, uint64_t balance) {
    std::vector<TraderId> ids;
    {
        std::unique_lock lock(mutex_);
        for (auto& [id, entry] : traders_) {
            if (entry.type == "noise") ids.push_back(id);
        }
    }
    while (ids.size() > n) {
        remove_trader(ids.back());
        ids.pop_back();
    }
    while (ids.size() < n) {
        ids.push_back(add_noise_trader("noise" + std::to_string(next_id_.load()), balance));
    }
}

void TraderRegistry::set_market_maker_spread(Price half_spread) {
    std::unique_lock lock(mutex_);
    for (auto& [id, entry] : traders_) {
        if (entry.type == "mm") {
            static_cast<MarketMaker*>(entry.trader.get())->set_half_spread(half_spread);
        }
    }
}

void TraderRegistry::set_tick_interval_ms(int ms) {
    tick_interval_ms_.store(ms);
}
