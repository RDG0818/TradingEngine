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
            std::lock_guard<std::mutex> lk(m_);
            accepted_ = true;
            cv_.notify_one();
        });
    }

    void waitForAcceptance() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this]{ return accepted_; });
    }

    void reset() {
        std::lock_guard<std::mutex> lk(m_);
        accepted_ = false;
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    bool accepted_ = false;
};

class LatencyBenchmarkFixture : public benchmark::Fixture {
public:
    std::unique_ptr<EventDispatcher> dispatcher;
    std::unique_ptr<MatchingEngine> engine;
    std::unique_ptr<LatencyListener> listener;

    void SetUp(const ::benchmark::State& state) override {
        dispatcher = std::make_unique<EventDispatcher>();
        engine = std::make_unique<MatchingEngine>(*dispatcher);
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
    static MatchingEngine engine(dispatcher);
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
