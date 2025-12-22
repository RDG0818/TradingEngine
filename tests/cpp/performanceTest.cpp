#include "benchmark/benchmark.h"
#include "matchingEngine.h"
#include "orderBook.h"
#include "eventDispatcher.h"
#include "symbolRegistry.h"
#include "orderFactory.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <string>

class BenchmarkFixture : public benchmark::Fixture {
public:
    EventDispatcher dispatcher;
    MatchingEngine engine;

    BenchmarkFixture() : engine(dispatcher) {}

    void SetUp(const ::benchmark::State& state) override {
        engine.start();
    }

    void TearDown(const ::benchmark::State& state) override {
        engine.stop();
    }
};

BENCHMARK_F(BenchmarkFixture, BM_OrderLatency)(benchmark::State& state) {
    std::mutex m;
    std::condition_variable cv;
    bool trade_executed = false;

    dispatcher.subscribe<TradeExecutedEvent>([&](const TradeExecutedEvent& event) {
        std::lock_guard<std::mutex> lk(m);
        trade_executed = true;
        cv.notify_one();
    });

    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "102.00", .quantity = 50, .trader_id = 999});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "101.50", .quantity = 200, .trader_id = 999});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 100, .trader_id = 999});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 1000000, .trader_id = 999});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 100, .trader_id = 998});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "98.50", .quantity = 200, .trader_id = 998});
    engine.submit_order({.symbol = "LAT", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "98.00", .quantity = 50, .trader_id = 998});
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 

    for (auto _ : state) {
        state.PauseTiming();
        {
            std::lock_guard<std::mutex> lk(m);
            trade_executed = false;
        }
        state.ResumeTiming();
        
        engine.submit_order({.symbol = "LAT", .order_type = OrderType::MARKET, .side = Side::BUY, .quantity = 1, .trader_id = 1});

        // Wait until the trade is executed
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&]{ return trade_executed; });
        }
    }
}

static void BM_ThreadedThroughput(benchmark::State& state) {
    // Shared state for all threads, initialized once
    static std::once_flag flag;
    static EventDispatcher dispatcher;
    static MatchingEngine engine(dispatcher);

    // Initialize and pre-fill the order book once
    std::call_once(flag, []() {
        engine.start();
        // Pre-fill the order book with depth to create a realistic market
        for (int i = 0; i < 10; ++i) {
            // Sell side (ask)
            engine.submit_order({.symbol = "THR_T", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = std::to_string(100.01 + i * 0.01), .quantity = 100, .trader_id = 999});
            // Buy side (bid)
            engine.submit_order({.symbol = "THR_T", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = std::to_string(99.99 - i * 0.01), .quantity = 100, .trader_id = 998});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Allow time for the order book to be built
    });

    // --- Per-thread setup ---
    // Each thread gets a unique role (buyer/seller) and price to create a mixed workload
    const bool is_buyer = (state.thread_index() % 2 == 0);
    const Side side = is_buyer ? Side::BUY : Side::SELL;
    const TraderID trader_id = static_cast<TraderID>(state.thread_index());

    // Pre-calculate the price string to avoid this work in the timed loop
    std::string price_str;
    if (is_buyer) {
        // Buyers will place orders at various prices, some matching, some not
        double price = 99.98 + (state.thread_index() / 2) * 0.01;
        price_str = std::to_string(price);
    } else {
        // Sellers do the same on the other side of the spread
        double price = 100.02 - ((state.thread_index() - 1) / 2) * 0.01;
        price_str = std::to_string(price);
    }
    // --- End per-thread setup ---

    // The timed loop: each thread executes this loop
    for (auto _ : state) {
        // This is the operation we want to measure the throughput of.
        engine.submit_order({.symbol = "THR_T", .order_type = OrderType::LIMIT, .side = side, .price = price_str, .quantity = 1, .trader_id = trader_id});
    }
    
    // Tell the framework how many items this thread processed.
    // It will be aggregated and reported as items/second.
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ThreadedThroughput)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16)
    ->UseRealTime();


BENCHMARK_MAIN();