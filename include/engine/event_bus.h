#pragma once
#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

using SubscriptionToken = uint64_t;

// Type-safe pub/sub keyed by std::type_index with handlers stored as
// std::any. Publish takes a shared lock so multiple publishers never
// block each other; subscribe/unsubscribe take an exclusive lock since
// those are the only operations that mutate the handler map.
class EventBus {
public:
    template<typename EventType>
    SubscriptionToken subscribe(std::function<void(const EventType&)> callback) {
        auto token = next_token_.fetch_add(1, std::memory_order_relaxed);
        std::unique_lock lock(mutex_);
        handlers_[typeid(EventType)].push_back({token,
            [cb = std::move(callback)](const std::any& e) {
                cb(std::any_cast<const EventType&>(e));
            }});
        return token;
    }

    void unsubscribe(SubscriptionToken token);

    template<typename EventType>
    void publish(const EventType& event) {
        std::shared_lock lock(mutex_);
        auto it = handlers_.find(typeid(EventType));
        if (it == handlers_.end()) return;
        std::any wrapped = event;
        for (const auto& h : it->second)
            h.fn(wrapped);
    }

private:
    struct Handler {
        SubscriptionToken token;
        std::function<void(const std::any&)> fn;
    };
    std::unordered_map<std::type_index, std::vector<Handler>> handlers_;
    mutable std::shared_mutex mutex_;
    std::atomic<SubscriptionToken> next_token_{1};
};
