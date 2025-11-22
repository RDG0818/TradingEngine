#include "benchmark/benchmark.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/orderFactory.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>

class LatencyTestListener : public EventListener {
public:
    std::atomic<bool> trade_executed{false};

    void onEvent(const Event& event) override {
        if (event.type == EventType::TRADE_EXECUTED) {
            trade_executed = true;
        }
    }
};

class BenchmarkFixture : public benchmark::Fixture {
public:
    EventDispatcher dispatcher;
    MatchingEngine engine;
    LatencyTestListener listener;

    BenchmarkFixture() : engine(dispatcher) {
        dispatcher.subscribe(EventType::TRADE_EXECUTED, &listener);
        SymbolRegistry::getInstance().registerSymbol("LAT");
        engine.start();
    }

    void TearDown(const benchmark::State& state) override {
        engine.stop();
    }
};

BENCHMARK_F(BenchmarkFixture, BM_OrderLatency)(benchmark::State& state) {
    RawOrderParams sell_params = {"LAT", 999, Side::SELL, OrderType::LIMIT, 50, 1020000};
    RawOrderParams sell_params2 = {"LAT", 999, Side::SELL, OrderType::LIMIT, 200, 1015000};
    RawOrderParams sell_params3 = {"LAT", 999, Side::SELL, OrderType::LIMIT, 100, 1010000};
    RawOrderParams sell_params4 = {"LAT", 999, Side::SELL, OrderType::LIMIT, 1000000, 1000000};
    
    engine.submitOrder(sell_params);
    engine.submitOrder(sell_params2);
    engine.submitOrder(sell_params3);
    engine.submitOrder(sell_params4);

    RawOrderParams buy_params1 = {"LAT", 998, Side::BUY, OrderType::LIMIT, 100, 990000};
    RawOrderParams buy_params2 = {"LAT", 998, Side::BUY, OrderType::LIMIT, 200, 985000};
    RawOrderParams buy_params3 = {"LAT", 998, Side::BUY, OrderType::LIMIT, 50, 980000};

    engine.submitOrder(buy_params1);
    engine.submitOrder(buy_params2);
    engine.submitOrder(buy_params3);

    for (auto _ : state) {
        state.PauseTiming();
        listener.trade_executed = false;
        state.ResumeTiming();

        RawOrderParams params = {"LAT", 1, Side::BUY, OrderType::MARKET, 1};
        engine.submitOrder(params);
        
        while (!listener.trade_executed) {
            // Busy wait
        }
    }
}

static void BM_ThreadedThroughput(benchmark::State& state) {
    EventDispatcher dispatcher;
    MatchingEngine engine(dispatcher);
    SymbolRegistry::getInstance().registerSymbol("THR_T");
    engine.start();

    // Pre-fill order book
    for (int i = 0; i < 100; ++i) {
        RawOrderParams sell_params = {"THR_T", 999, Side::SELL, OrderType::LIMIT, 100, static_cast<Price>(10001 + i * 1)};
        engine.submitOrder(sell_params);
        RawOrderParams buy_params = {"THR_T", 998, Side::BUY, OrderType::LIMIT, 100, static_cast<Price>(9999 - i * 1)};
        engine.submitOrder(buy_params);
    }

    std::vector<std::thread> threads;
    threads.reserve(state.threads());

    for (auto _ : state) {
        for (int i = 0; i < state.threads(); ++i) {
            threads.emplace_back([&] {
                RawOrderParams params = {"THR_T", (TraderID)i, Side::BUY, OrderType::MARKET, (Quantity)state.range(0)};
                engine.submitOrder(params);
            });
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
    }
    
    engine.stop();
}
BENCHMARK(BM_ThreadedThroughput)->Threads(8)->Arg(1000);
