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
#include <numeric>
#include <algorithm>
#include <cmath>

class PerformanceTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MatchingEngine engine;

    PerformanceTest() : engine(dispatcher) {}

    void SetUp() override {
        engine.start();
    }

    void TearDown() override {
        engine.stop();
    }
};

class LatencyListener {
public:
    std::vector<std::chrono::high_resolution_clock::time_point> trade_timestamps;
    std::mutex mtx;
    std::condition_variable cv;

    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            trade_timestamps.push_back(std::chrono::high_resolution_clock::now());
            cv.notify_one();
        });
    }

    void waitForTrades(size_t count) {
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&]{ return trade_timestamps.size() >= count; })) {
            FAIL() << "Timeout waiting for trades. Expected " << count << ", but got " << trade_timestamps.size();
        }
    }
};

TEST_F(PerformanceTest, LatencyBenchmark) {
    LatencyListener listener;
    listener.subscribe(dispatcher);

    const int NUM_ORDERS_TO_TEST = 10000;
    std::vector<long long> latencies_nanos;
    latencies_nanos.reserve(NUM_ORDERS_TO_TEST);

    engine.submitOrder({.symbol = "LAT", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = (Quantity)NUM_ORDERS_TO_TEST, .traderID = 999});
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < NUM_ORDERS_TO_TEST; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        engine.submitOrder({.symbol = "LAT", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 1, .traderID = 1});
        
        listener.waitForTrades(i + 1);
        auto end_time = listener.trade_timestamps[i];

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        latencies_nanos.push_back(duration.count());
    }

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

TEST_F(PerformanceTest, ThroughputStressTest) {
    std::atomic<int> trade_counter(0);
    dispatcher.subscribe<TradeExecutedEvent>([&](const TradeExecutedEvent& event) {
        trade_counter.fetch_add(1, std::memory_order_relaxed);
    });

    const int NUM_ORDERS_TO_SUBMIT = 1000000;
    const int NUM_THREADS = 4;
    const int ORDERS_PER_THREAD = NUM_ORDERS_TO_SUBMIT / NUM_THREADS;

    engine.submitOrder({.symbol = "PERF", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = (Quantity)NUM_ORDERS_TO_SUBMIT, .traderID = 999});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ORDERS_PER_THREAD; ++j) {
                engine.submitOrder({.symbol = "PERF", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 1, .traderID = 1});
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
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    double seconds = duration.count() / 1e9;
    double throughput = NUM_ORDERS_TO_SUBMIT / seconds;

    std::cout << "\n--- Performance Benchmark Results ---" << std::endl;
    std::cout << "Processed " << NUM_ORDERS_TO_SUBMIT << " orders in " << seconds << " seconds." << std::endl;
    std::cout << "Throughput: " << static_cast<int>(throughput) << " orders/sec" << std::endl;
    std::cout << "-------------------------------------\n" << std::endl;

    ASSERT_GT(throughput, 0);
}
