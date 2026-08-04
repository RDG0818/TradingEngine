#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>
#include "include/event_bus.h"
#include "include/exchange.h"
#include "include/core/order.h"
#include "include/exchange_events.h"

// ─── Shared order ID counter ───────────────────────────────────────────────
static std::atomic<OrderId> g_oid{500000};

// ─── BM_Throughput ─────────────────────────────────────────────────────────
// Measures how many limit orders/sec the matcher can accept.
// Orders do not match (bids well below asks) so the book fills up.
static void BM_Throughput(benchmark::State& state) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderId tid = 9001;

    for (auto _ : state) {
        OrderId oid = g_oid.fetch_add(1);
        matcher.submit(LimitOrder{oid, tid, Side::Buy,
                                  600000000,  // $60,000 — far from any ask
                                  1, TimeInForce::GTC, {}});
    }

    // Drain the queue before stopping
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    matcher.stop();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Throughput)->Iterations(50000)->Unit(benchmark::kMicrosecond);

// ─── BM_MatchLatency ───────────────────────────────────────────────────────
// Measures end-to-end latency from submit → FillEvent for a matched order.
// Collects N latencies and reports p50/p95/p99/p999.
static void BM_MatchLatency(benchmark::State& state) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    const TraderId maker_id = 9002;
    const TraderId taker_id = 9003;
    const int N = 10000;

    std::vector<int64_t> latencies_us;
    latencies_us.reserve(N);

    std::atomic<bool> fill_received{false};
    auto token = bus.subscribe<FillEvent>([&](const FillEvent&) {
        fill_received.store(true, std::memory_order_release);
    });

    for (auto _ : state) {
        for (int i = 0; i < N; i++) {
            fill_received.store(false, std::memory_order_release);

            // Place resting ask
            OrderId ask_id = g_oid.fetch_add(1);
            matcher.submit(LimitOrder{ask_id, maker_id, Side::Sell,
                                       640100000, 1, TimeInForce::GTC, {}});
            std::this_thread::sleep_for(std::chrono::microseconds(150));

            auto t0 = std::chrono::steady_clock::now();
            OrderId bid_id = g_oid.fetch_add(1);
            matcher.submit(LimitOrder{bid_id, taker_id, Side::Buy,
                                       640200000, 1, TimeInForce::IOC, {}});

            while (!fill_received.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            auto t1 = std::chrono::steady_clock::now();
            latencies_us.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
        }
    }

    bus.unsubscribe(token);
    matcher.stop();

    // Report percentiles
    std::sort(latencies_us.begin(), latencies_us.end());
    size_t n = latencies_us.size();
    if (n > 0) {
        state.counters["p50_us"]  = latencies_us[n * 50  / 100];
        state.counters["p95_us"]  = latencies_us[n * 95  / 100];
        state.counters["p99_us"]  = latencies_us[n * 99  / 100];
        state.counters["p999_us"] = latencies_us[n * 999 / 1000];
    }
    state.SetItemsProcessed(N * state.iterations());
}
BENCHMARK(BM_MatchLatency)->Iterations(1)->Unit(benchmark::kMicrosecond);

// ─── BM_Contention ─────────────────────────────────────────────────────────
// Measures throughput under concurrent read (book snapshot) + write (submit).
static void BM_Contention_Writers(benchmark::State& state) {
    static Exchange* exchange_ptr = nullptr;
    static std::once_flag init_flag;

    std::call_once(init_flag, [&]() {
        static Exchange ex;
        ex.start(640000000);
        exchange_ptr = &ex;
    });

    TraderId tid = 9010 + state.thread_index();

    for (auto _ : state) {
        OrderId oid = g_oid.fetch_add(1);
        exchange_ptr->submit_order(LimitOrder{
            oid, tid, Side::Buy, 600000000, 1, TimeInForce::GTC, {}
        });
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_Contention_Readers(benchmark::State& state) {
    static Exchange ex_readers;
    static std::once_flag reader_init;
    std::call_once(reader_init, [&]() { ex_readers.start(640000000); });

    for (auto _ : state) {
        benchmark::DoNotOptimize(ex_readers.book_snapshot());
    }
    state.SetItemsProcessed(state.iterations());
}

// 4 writer threads
BENCHMARK(BM_Contention_Writers)->Threads(4)->Iterations(10000)->Unit(benchmark::kMicrosecond);
// 8 reader threads
BENCHMARK(BM_Contention_Readers)->Threads(8)->Iterations(10000)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
