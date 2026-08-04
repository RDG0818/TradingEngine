# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Talat

A high-performance simulated trading exchange ("Talat" = market in Thai). Pure C++20 matching engine with a terminal UI — designed as a systems/quant interview portfolio piece showcasing lock-free concurrency, market microstructure, and real-time rendering.

## Build & Run

**Prerequisites:** C++20 compiler, CMake 3.14+, Boost 1.74+

```bash
# Build everything (fetches ftxui via CMake FetchContent on first run)
make build

# Run the TUI
make run           # ./build/trading_engine
make run -- --seed 50000   # seed at $50,000

# Tests
make test          # runs C++ test suite (48 tests, 12 suites)
./build/tests      # same, directly

# Benchmarks (throughput, match latency, contention)
make bench         # builds + runs ./build/benchmarks
```

## Architecture

### Data Flow
```
./build/trading_engine (C++ binary)
    ├── Exchange
    │     ├── OrderMatcher    (worker thread, moodycamel lock-free queue)
    │     ├── OrderBook       (boost::container::flat_map, bid/ask levels)
    │     ├── TraderRegistry  (tick thread, 3 trader types, owns LatentPrice)
    │     └── EventBus        (type-safe pub/sub, shared_mutex)
    └── TUI (ftxui, fullscreen terminal)
          ├── Order Book panel  (depth bars, 6 bid/ask levels)
          ├── Recent Fills panel (user fills highlighted)
          ├── Stats panel       (p50/p99 latency, throughput, book depth)
          └── Command bar       (order entry + slash commands)
```

### C++ Core (`src/`, `include/`)

- **Exchange** (`include/engine/exchange.h`) — top-level orchestrator. Owns `OrderMatcher`, `TraderRegistry`, `EventBus`, and user `Portfolio` map. Key methods: `start(seed_price)`, `stop()`, `submit_order()`, `cancel_order()`, `book_snapshot()`, `portfolio_snapshot()`, `registry()`.
- **OrderMatcher** (`include/engine/order_matcher.h`) — processes orders on a dedicated worker thread using moodycamel `ConcurrentQueue<Command>`. Handles all four order types (Limit, Market, StopLimit, StopMarket) with GTC/IOC/FOK time-in-force. Self-match prevention. Stop orders triggered by last trade price.
- **OrderBook** (`include/engine/order_book.h`) — `boost::container::flat_map<Price, PriceLevel>` for cache-friendly sorted levels. `shared_mutex` for concurrent reads. Snapshot-before-callback pattern in walk methods to prevent deadlocks.
- **EventBus** (`include/engine/event_bus.h`) — type-safe pub/sub using `std::type_index` + `std::any`. `shared_mutex` for concurrent publish. Subscription tokens for cleanup. Events: `FillEvent`, `BookUpdateEvent`, `OrderAcceptedEvent`, `OrderRejectedEvent`, `OrderCancelledEvent`.
- **TraderRegistry** (`include/market/trader_registry.h`) — owns `LatentPrice`, runs 3 trader types on a configurable tick thread (default 200ms). Methods: `add_market_maker/informed_trader/noise_trader`, `pause_all/resume_all`, `set_market_maker_count/informed_count/noise_count`, `set_sigma`, `set_market_maker_spread`, `set_tick_interval_ms`.
- **LatentPrice** (`include/market/latent_price.h`) — header-only GBM fair value. Zero-drift, configurable σ (default 0.0003). `std::atomic<Price>` for thread-safe reads. `tick()` advances one GBM step.
- **Portfolio** — per-trader balance, position, avg cost, unrealized PnL. Thread-safe with `std::mutex`.
- **StatsTracker** (`include/engine/stats_tracker.h`) — header-only rolling 5-second window. Subscribes to `OrderAcceptedEvent`. `snapshot()` returns p50/p99 latency (µs) and orders/sec.

### Trader Types (`include/market/traders/`)

| Trader | Behavior |
|--------|----------|
| `MarketMaker` | Quotes bid+ask ± latent price each tick. Tracks fill rate over 20-tick window; widens spread up to 3× when fill_rate > 30% (Glosten-Milgrom adverse selection). Cancels and requotes on every tick. |
| `InformedTrader` | Noisy signal = latent × (1 + N(0, σ)). Submits IOC limit at signal price when signal diverges from last_price by more than threshold (default 0.2%). Drives price discovery. |
| `NoiseTrader` | Poisson arrivals (λ=0.7 default). 60% limit / 40% market split. Log-normal sizes. Random side. Provides uninformed liquidity. |

### TUI (`include/tui/tui.h`, `src/tui/tui.cpp`)

Built with ftxui v5.0.0 (fetched via CMake FetchContent). Three panels:
- **Order Book** — 6 bid/6 ask levels with ASCII depth bars. User's resting prices highlighted.
- **Recent Fills** — last 20 trades. User fills highlighted in bold.
- **Stats** — live p50/p99 latency, throughput, book depth, user position.

Command bar at the bottom. Commands:
- `buy <qty> @ <price>` — limit buy (GTC)
- `sell <qty> @ <price>` — limit sell (GTC)
- `buy <qty>` / `sell <qty>` — market order (IOC)
- `cancel <order_id>` — cancel resting order
- `q` / `quit` — exit

Slash commands:
- `/vol <0-1>` — set GBM volatility σ
- `/spread <dollars>` — set market maker half-spread in dollars
- `/speed <1-10>` — set tick speed (1=slow/2s, 10=fast/200ms)
- `/pause` / `/resume` — pause/resume all automated traders
- `/traders <mm|informed|noise> <n>` — set trader count for each type
- `/help` — show command reference

### Prices

`uint64_t` fixed-point where `10000 = $1.00` (e.g. $64,200 = 642,000,000).

### Directory Layout

`include/` and `src/` are organized by subsystem, mirrored between the two:

- `core/` — pure data types with zero dependencies on other subsystems (`order.h`: `Order` variant, `Side`, `TimeInForce`, `Fill`, price/qty typedefs).
- `engine/` — the exchange itself: `order_book.h`, `exchange_events.h`, `order_matcher.h`, `event_bus.h`, `exchange.h`, `portfolio.h`, `stats_tracker.h`.
- `market/` — the simulated ecosystem trading against the engine: `latent_price.h`, `trader.h`, `trader_registry.h`, `traders/{market_maker,informed_trader,noise_trader}.h`.
- `tui/` — the ftxui frontend: `tui.h`, `order_command_parser.h`.

### Include Path Convention

Two CMake targets with different roots:
- `core_lib` / `trading_engine` have `${PROJECT_SOURCE_DIR}/include` in their include path → source files use `#include "core/order.h"`, `#include "engine/exchange.h"`, `#include "market/traders/market_maker.h"` etc. — always relative to the `include/` root, never relative to the including file's own directory.
- `tests` target has `${PROJECT_SOURCE_DIR}` (repo root) → test files use `#include "include/core/order.h"`, `#include "include/engine/exchange.h"` etc.
