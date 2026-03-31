// src/trader_registry.cpp
#include "trader_registry.h"
#include "exchange_events.h"
#include <chrono>
#include <algorithm>

TraderRegistry::TraderRegistry(OrderMatcher& matcher) : matcher_(matcher) {}

TraderRegistry::~TraderRegistry() { stop(); }

void TraderRegistry::start() {
    running_ = true;
    tick_thread_ = std::thread(&TraderRegistry::tick_loop, this);
}

void TraderRegistry::stop() {
    if (running_.exchange(false) && tick_thread_.joinable())
        tick_thread_.join();
}

void TraderRegistry::remove_trader(TraderId id) {
    std::unique_lock lock(mutex_);
    traders_.erase(id);
    active_traders_.erase(id);
    type_names_.erase(id);
}

void TraderRegistry::start_trader(TraderId id) {
    std::unique_lock lock(mutex_);
    if (active_traders_.count(id)) active_traders_[id] = true;
}

void TraderRegistry::stop_trader(TraderId id) {
    std::unique_lock lock(mutex_);
    if (active_traders_.count(id)) active_traders_[id] = false;
}

void TraderRegistry::tick_loop() {
    using namespace std::chrono_literals;
    while (running_) {
        {
            std::unique_lock lock(mutex_);
            for (auto& [id, trader] : traders_) {
                if (!active_traders_[id]) continue;
                trader->tick(last_price_,
                    [this](Order o) { matcher_.submit(std::move(o)); },
                    [](OrderId) -> bool { return false; });
            }
        }
        std::this_thread::sleep_for(10ms);
    }
}

void TraderRegistry::subscribe_to_fills(EventBus& bus) {
    bus.subscribe<FillEvent>([this](const FillEvent& e) {
        std::unique_lock lock(mutex_);
        last_price_ = e.fill.fill_price;
        // Notify both maker and taker traders.
        for (TraderId tid : {e.fill.maker_trader_id, e.fill.taker_trader_id}) {
            auto it = traders_.find(tid);
            if (it != traders_.end()) it->second->on_fill(e.fill);
        }
    });
}

std::optional<TraderInfo> TraderRegistry::trader_info(TraderId id) const {
    std::unique_lock lock(mutex_);
    auto it = traders_.find(id);
    if (it == traders_.end()) return std::nullopt;
    return TraderInfo{id, it->second->name(), type_names_.at(id),
                      active_traders_.at(id), it->second->metrics()};
}

std::vector<TraderInfo> TraderRegistry::all_traders() const {
    std::unique_lock lock(mutex_);
    std::vector<TraderInfo> result;
    for (const auto& [id, trader] : traders_)
        result.push_back({id, trader->name(), type_names_.at(id),
                          active_traders_.at(id), trader->metrics()});
    return result;
}
