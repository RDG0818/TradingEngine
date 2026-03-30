# Talat: TUI Pivot Design

**Date:** 2026-03-30
**Status:** Approved

## Summary

Pivot Talat from a web-app-centric project to a focused C++ portfolio piece: a high-performance matching engine with a clean terminal UI and a rigorous benchmark harness. Target audience is systems and quant interviews — the artifact should be a pure C++ binary that demonstrates design depth, not a web stack.

---

## What Gets Removed

- `backend/` — FastAPI app, Python bindings, pybind11 CMake target, requirements.txt
- `frontend/` — React/TypeScript app
- `src/python_bindings.cpp`
- All 8 current trader implementations (`market_maker`, `momentum`, `mean_reversion`, `twap`, `trend_follower`, `random_limit`, `random_market`, `panic`)
- Market events system (`include/market_events.h`, `src/market_events.cpp`)
- `TraderRegistry` — rebuilt from scratch (simpler, no market events)

## What Gets Kept

- `OrderBook`, `OrderMatcher`, `EventBus`, `Exchange`, `Portfolio` — unchanged
- `include/order.h`, `include/exchange_events.h` — unchanged
- All existing tests (38 tests, 14 suites) — must stay green throughout

The `Exchange` class gets minor cleanup: remove Python callback hooks (`on_fill_callback`, `on_book_update_callback`) in favor of direct EventBus subscriptions. Everything else stays.

---

## New Market Model

### Shared Latent Price

A `LatentPrice` component owns the "true" fair value of BTC. It ticks on a configurable timer and steps via GBM:

```
S(t+dt) = S(t) * exp(σ * sqrt(dt) * Z)    Z ~ N(0,1)
```

- Zero drift (fair market)
- σ configurable at runtime via `/vol` command
- Stored as `std::atomic<Price>` — reads are cheap for all traders

This is the missing shared reference that the current design lacks. All three trader types read from it.

### Trader Types

**`MarketMaker`**
- Quotes bid and ask symmetrically around the latent price each tick
- Base half-spread is configurable (`/spread`)
- Tracks recent fill rate; widens spread proportionally when getting hit frequently (adverse selection detection, Glosten-Milgrom intuition)
- Cancels all resting quotes and requotes every tick
- One instance by default

**`InformedTrader`**
- Periodically samples a noisy signal: `signal = latent_price + ε`, `ε ~ N(0, σ_noise)`
- When `|signal - mid_price| > threshold`, submits an aggressive limit order toward fair value
- Signal frequency and noise level are fixed at construction
- Creates the price discovery mechanism — this is what makes the chart move realistically
- 2 instances by default

**`NoiseTrader`**
- Poisson-distributed arrivals (λ configurable)
- Side: uniform random
- Size: log-normal distribution
- Order type: 60% limit (near the touch), 40% market
- Provides uninformed flow; makes market making profitable and adds realistic volume
- 3 instances by default

No other trader types. No market events.

### Simplified TraderRegistry

- Owns the `LatentPrice` and all trader instances
- Single tick thread at configurable rate
- `add<T>(...)`, `remove(id)`, `start(id)`, `stop(id)`, `set_count<T>(n)` (for `/traders` command)
- No market event machinery

---

## TUI

Built with **ftxui** via CMake `FetchContent` (no new git submodule). Three-panel layout with a command bar:

```
┌─ TALAT ── BTC $64,230.15 ──────────── Spread $1.20 ── 2,847 ord/s ─┐
│                                                                       │
│   ORDER BOOK          RECENT FILLS          STATS                    │
│   ──────────          ────────────          ──────                   │
│   64,235  0.42 ░░     64,232  0.10  buy     p50 latency:   8 µs     │
│   64,232  1.20 ░░░░   64,228  0.25  sell    p99 latency:  42 µs     │
│   64,231  0.85 ░░░    64,231  0.10  buy     throughput: 2.8k/s      │
│   ── mid ──────────   64,229  0.50  sell    orders in book:  47     │
│   64,228  0.95 ░░░    64,230  0.20  buy     your position:   0      │
│   64,226  1.40 ░░░░░  64,227  0.15  sell    your fills:      0      │
│   64,223  0.60 ░░                                                    │
│                                                                       │
├───────────────────────────────────────────────────────────────────────┤
│  > _                                                                  │
└───────────────────────────────────────────────────────────────────────┘
```

**Order commands** (typed at `>` prompt):
- `buy <qty> @ <price>` — limit buy
- `sell <qty> @ <price>` — limit sell
- `buy <qty>` — market buy
- `sell <qty>` — market sell
- `cancel <order_id>` — cancel resting order

**Slash commands** (runtime customization):
- `/vol <0.0–1.0>` — adjust GBM volatility
- `/spread <ticks>` — adjust market maker base half-spread
- `/speed <1–10>` — scale trader tick rate
- `/pause` / `/resume` — freeze/resume automated market
- `/traders <mm|informed|noise> <count>` — adjust trader instance count
- `/help` — list commands

User's own resting orders are highlighted in the book. User fills flash briefly. Stats panel updates live: throughput and latency are computed from a rolling 5-second window of submit-to-event timestamps recorded by an `EventBus` subscriber inside the exchange, not from the benchmark binary.

---

## Benchmark Harness

Standalone `./build/benchmarks` binary (expanded from existing). Three scenarios:

**Throughput** — floods the matcher with limit orders from multiple threads, measures sustained and peak orders/second until steady state.

**Latency** — submits a single order, waits for `OrderAcceptedEvent` or `FillEvent`, records round-trip. 100k iterations. Reports p50/p95/p99/p999 in microseconds.

**Contention** — concurrent readers (book snapshots) + writers (order submissions) running simultaneously. Measures throughput degradation and latency increase vs the single-threaded baseline. Demonstrates the `shared_mutex` trade-off on the book.

Example output:
```
=== Talat Benchmark ===

Throughput
  sustained:    312,450 orders/sec
  peak:         489,200 orders/sec

Latency (submit → accepted/fill)
  p50:    6 µs
  p95:   18 µs
  p99:   41 µs
  p999: 112 µs

Contention (8 readers, 4 writers)
  throughput:   198,300 orders/sec  (-37% vs baseline)
  p99 latency:  67 µs               (+63% vs baseline)
```

---

## Build & CMake

Two primary targets, one optional:

- `trading_engine` — TUI binary, links `core_lib` + ftxui
- `tests` — unchanged
- `benchmarks` — expanded, still `EXCLUDE_FROM_ALL`

ftxui added via `FetchContent_Declare` in CMakeLists.txt. No conda env required. No Python dependency.

```bash
make build                   # compiles core_lib + trading_engine + tests
./build/trading_engine       # launch TUI
./build/tests                # run all tests
./build/benchmarks           # run benchmark suite
```

Startup: seeds latent price from CoinGecko if network available, falls back to a hardcoded default.

---

## Implementation Order

1. Remove backend, frontend, python_bindings, old traders, market events from CMake and filesystem
2. Strip Exchange of Python callback hooks; verify all 38 tests still pass
3. Implement `LatentPrice` + new `MarketMaker`, `InformedTrader`, `NoiseTrader`
4. Rebuild `TraderRegistry` (simplified)
5. Expand benchmark harness
6. Build TUI with ftxui (observer mode first, then command input)
7. Wire slash commands
