#include "gtest/gtest.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderBook.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/limitOrder.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/orderFactory.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>

// A listener that captures the exact timestamp of a trade event.
class LatencyListener {
public:
    std::vector<std::chrono::high_resolution_clock::time_point> trade_timestamps;

    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            trade_timestamps.push_back(std::chrono::high_resolution_clock::now());
        });
    }
};

TEST(PerformanceTest, LatencyBenchmark) {
    // --- SETUP ---
    OrderBook book;
    EventDispatcher dispatcher;
    MatchingEngine engine(book, dispatcher);

    LatencyListener listener;
    listener.subscribe(dispatcher);

    engine.start();

    // --- ARRANGE ---
    const int NUM_ORDERS_TO_TEST = 10000;
    std::vector<long long> latencies_nanos;
    latencies_nanos.reserve(NUM_ORDERS_TO_TEST);

    // Pre-load the book with a large resting order to ensure instant matches.
    engine.submitOrder({.symbol = "LAT", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = (Quantity)NUM_ORDERS_TO_TEST, .traderID = 999});
    engine.cancelOrder(0); // Unblock the queue

    // Wait for the engine to process the setup order
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    listener.trade_timestamps.clear();

    // --- ACT & MEASURE ---
    for (int i = 0; i < NUM_ORDERS_TO_TEST; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        engine.submitOrder({.symbol = "LAT", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 1, .traderID = 1});
        engine.cancelOrder(0);

        while (listener.trade_timestamps.size() <= i) {
            std::this_thread::yield();
        }
        auto end_time = listener.trade_timestamps[i];

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        latencies_nanos.push_back(duration.count());
    }

    engine.stop();

    // --- ANALYZE & REPORT ---
    std::sort(latencies_nanos.begin(), latencies_nanos.end());

    long long sum = std::accumulate(latencies_nanos.begin(), latencies_nanos.end(), 0LL);
    double mean = static_cast<double>(sum) / latencies_nanos.size();
    
    long long p50 = latencies_nanos[latencies_nanos.size() * 0.50];
    long long p90 = latencies_nanos[latencies_nanos.size() * 0.90];
    long long p99 = latencies_nanos[latencies_nanos.size() * 0.99];

    std::cout << "\n--- Latency Benchmark Results ---" << std::endl;
    std::cout << "Tested " << NUM_ORDERS_TO_TEST << " individual orders." << std::endl;
    std::cout << "Mean Latency: " << mean / 1000.0 << " µs (microseconds)" << std::endl;
    std::cout << "p50 (Median): " << p50 / 1000.0 << " µs" << std::endl;
    std::cout << "p90 Latency:  " << p90 / 1000.0 << " µs" << std::endl;
    std::cout << "p99 Latency:  " << p99 / 1000.0 << " µs" << std::endl;
    std::cout << "---------------------------------\n" << std::endl;

    ASSERT_GT(mean, 0);
}