// TODO: rewrite for new API (Exchange, OrderMatcher, OrderBook) — excluded from default build via EXCLUDE_FROM_ALL
#include "benchmark/benchmark.h"
#include "matchingEngine.h"
#include "orderBook.h"
#include "eventDispatcher.h"
#include "symbolRegistry.h"
#include "orderFactory.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>

// --- Test #1: Submission Latency Benchmark ---
// This test needs a fresh engine and listener for each run to measure single events accurately.

class LatencyListener {
public:
    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<OrderAcceptedEvent>([this](const OrderAcceptedEvent& event) {
            accepted_.store(true, std::memory_order_release);
        });
    }

    void waitForAcceptance() {
        while (!accepted_.load(std::memory_order_acquire)) {
            // Spin-wait
        }
    }

    void reset() {
        accepted_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> accepted_{false};
};

class LatencyBenchmarkFixture : public benchmark::Fixture {
public:
    std::unique_ptr<EventDispatcher> dispatcher;
    std::unique_ptr<OrderIdGenerator> order_id_generator;
    std::unique_ptr<MatchingEngine> engine;
    std::unique_ptr<LatencyListener> listener;

    void SetUp(const ::benchmark::State& state) override {
        dispatcher = std::make_unique<EventDispatcher>();
        order_id_generator = std::make_unique<OrderIdGenerator>();
        engine = std::make_unique<MatchingEngine>(*dispatcher, *order_id_generator);
        listener = std::make_unique<LatencyListener>();
        
        listener->subscribe(*dispatcher);
        engine->start();
    }

    void TearDown(const ::benchmark::State& state) override {
        engine->stop();
    }
};

BENCHMARK_F(LatencyBenchmarkFixture, BM_SubmissionLatency)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        listener->reset();
        state.ResumeTiming();
        
        engine->submit_order({.symbol = "LAT_SYM", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 1, .trader_id = 1});
        
        listener->waitForAcceptance();
    }
}


static void BM_MatchingThroughput(benchmark::State& state) {
    static std::once_flag flag;
    static EventDispatcher dispatcher;
    static OrderIdGenerator order_id_generator;
    static MatchingEngine engine(dispatcher, order_id_generator);
    static LatencyListener listener; 

    std::call_once(flag, []() {
        listener.subscribe(dispatcher);
        engine.start();
        for (int i = 0; i < 100; ++i) {
            engine.submit_order({.symbol = "THR_SYM", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = format_price(1010000 + i * 100), .quantity = 100, .trader_id = 999});
            engine.submit_order({.symbol = "THR_SYM", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = format_price(990000 - i * 100), .quantity = 100, .trader_id = 998});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
    });

    const TraderID trader_id = static_cast<TraderID>(state.thread_index());

    for (auto _ : state) {
        if (state.thread_index() % 2 == 0) {
            engine.submit_order({.symbol = "THR_SYM", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "105.00", .quantity = 1, .trader_id = trader_id});
        } else {
            engine.submit_order({.symbol = "THR_SYM", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "95.00", .quantity = 1, .trader_id = trader_id});
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_MatchingThroughput)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16)
    ->UseRealTime();

BENCHMARK_MAIN();
