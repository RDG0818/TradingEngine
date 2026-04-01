#pragma once
#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <vector>
#include "event_bus.h"
#include "exchange_events.h"

class StatsTracker {
public:
    struct Snapshot {
        uint64_t p50_us{0};
        uint64_t p99_us{0};
        double orders_per_sec{0.0};
    };

    explicit StatsTracker(EventBus& bus) {
        tokens_.push_back(bus.subscribe<OrderAcceptedEvent>([this](const OrderAcceptedEvent& e) {
            auto now = Clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now - e.submit_tp).count();
            if (latency_us >= 0) {
                std::lock_guard lock(mutex_);
                prune(now);
                latencies_.push_back({now, static_cast<uint64_t>(latency_us)});
                order_times_.push_back(now);
            }
        }));
    }

    Snapshot snapshot() const {
        std::lock_guard lock(mutex_);
        auto now = Clock::now();

        size_t recent_orders = 0;
        for (const auto& tp : order_times_) {
            if (now - tp <= WINDOW) recent_orders++;
        }
        double ops = static_cast<double>(recent_orders) /
                     std::chrono::duration<double>(WINDOW).count();

        if (latencies_.empty()) return {0, 0, ops};

        std::vector<uint64_t> vals;
        vals.reserve(latencies_.size());
        for (const auto& r : latencies_) {
            if (now - r.ts <= WINDOW) vals.push_back(r.latency_us);
        }
        if (vals.empty()) return {0, 0, ops};

        std::sort(vals.begin(), vals.end());
        uint64_t p50 = vals[vals.size() * 50 / 100];
        uint64_t p99 = vals[vals.size() * 99 / 100];

        return {p50, p99, ops};
    }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr auto WINDOW = std::chrono::seconds(5);

    struct LatencyRecord { Clock::time_point ts; uint64_t latency_us; };

    void prune(const Clock::time_point& now) {
        while (!latencies_.empty() && now - latencies_.front().ts > WINDOW)
            latencies_.pop_front();
        while (!order_times_.empty() && now - order_times_.front() > WINDOW)
            order_times_.pop_front();
    }

    mutable std::mutex mutex_;
    std::deque<LatencyRecord> latencies_;
    std::deque<Clock::time_point> order_times_;
    std::vector<SubscriptionToken> tokens_;
};
