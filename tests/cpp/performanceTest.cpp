#include "gtest/gtest.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderBook.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/limitOrder.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/orderFactory.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

// A minimal listener for performance testing.
class PerformanceListener {
private:
    std::atomic<int>& trade_counter;

public:
    PerformanceListener(std::atomic<int>& counter) : trade_counter(counter) {}

    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            trade_counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
};

TEST(PerformanceTest, ThroughputStressTest) {
    // --- SETUP ---
    OrderBook book;
    EventDispatcher dispatcher;
    MatchingEngine engine(book, dispatcher);

    std::atomic<int> trade_counter(0);
    PerformanceListener listener(trade_counter);
    listener.subscribe(dispatcher);

    engine.start();

    // --- ARRANGE ---
    const int NUM_ORDERS_TO_SUBMIT = 1000000;
    const int NUM_THREADS = 4;
    const int ORDERS_PER_THREAD = NUM_ORDERS_TO_SUBMIT / NUM_THREADS;

    // Pre-load the book with a huge resting sell order so all our buy orders will match.
    engine.submitOrder({.symbol = "PERF", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = (Quantity)NUM_ORDERS_TO_SUBMIT, .traderID = 999});
    engine.cancelOrder(0); // Unblock the queue

    // Give the engine a moment to process the resting order
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- ACT ---
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ORDERS_PER_THREAD; ++j) {
                engine.submitOrder({.symbol = "PERF", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 1, .traderID = 1});
                engine.cancelOrder(0);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    while (trade_counter.load() < NUM_ORDERS_TO_SUBMIT) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    engine.stop();

    // --- ANALYZE & ASSERT ---
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    double seconds = duration.count() / 1e9;
    double throughput = NUM_ORDERS_TO_SUBMIT / seconds;

    std::cout << "\n--- Performance Benchmark Results ---" << std::endl;
    std::cout << "Processed " << NUM_ORDERS_TO_SUBMIT << " orders in " << seconds << " seconds." << std::endl;
    std::cout << "Throughput: " << static_cast<int>(throughput) << " orders/sec" << std::endl;
    std::cout << "-------------------------------------\n" << std::endl;

    ASSERT_GT(throughput, 0);
}
