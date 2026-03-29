# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Talat

A high-performance simulated trading exchange ("Talat" = market in Thai). C++ core matching engine with Python (pybind11) bindings, a FastAPI backend, and a React/TypeScript frontend. Single synthetic symbol (BTC, seeded from CoinGecko at startup).

## Build & Run

**Prerequisites:** C++20 compiler, CMake 3.12+, Boost 1.74+, Python 3.10, Conda

```bash
# First-time setup
git submodule update --init --recursive
conda create -n talat python=3.10 && conda activate talat
cd backend && pip install -r requirements.txt && cd ..

# Build C++ core + Python bindings
make build        # cmake + compile + copies .so to backend/

# Run backend (from repo root, with conda env active)
cd backend && python3 -m uvicorn main:app --reload

# Run frontend
cd frontend/trading_exchange_frontend && npm run dev
```

**Tests:**
```bash
make test              # runs both C++ tests and pytest
./build/tests          # C++ tests only (38 tests, 14 suites)
./build/benchmarks     # performance benchmarks (EXCLUDE_FROM_ALL)
```

## Architecture

### Data Flow
```
React frontend (port 5173)
    ↓ HTTP REST + WebSocket /ws
FastAPI backend (Python, backend/main.py)
    ↓ pybind11 (.so module)
C++ Exchange
    ├── OrderMatcher    (worker thread, moodycamel lock-free queue)
    ├── OrderBook       (boost::container::flat_map, bid/ask levels)
    ├── TraderRegistry  (tick thread, 8 trader types)
    └── EventBus        (type-safe pub/sub, shared_mutex)
```

### C++ Core (`src/`, `include/`)

- **Exchange** — top-level orchestrator, the only class exposed to Python via pybind11 (`src/python_bindings.cpp`). Owns `OrderMatcher`, `TraderRegistry`, `EventBus`, and user `Portfolio` map. Python callback hooks: `on_fill_callback`, `on_book_update_callback`.
- **OrderMatcher** — processes orders on a dedicated worker thread using moodycamel `ConcurrentQueue<Command>`. Handles all four order types (Limit, Market, StopLimit, StopMarket) with GTC/IOC/FOK time-in-force. Self-match prevention. Stop orders triggered by last trade price.
- **OrderBook** — `boost::container::flat_map<Price, PriceLevel>` for cache-friendly sorted levels. `shared_mutex` for concurrent reads. Snapshot-before-callback pattern in walk methods to prevent deadlocks.
- **EventBus** — type-safe pub/sub using `std::type_index` + `std::any`. `shared_mutex` for concurrent publish. Subscription tokens for cleanup. Events: `FillEvent`, `BookUpdateEvent`, `OrderAcceptedEvent`, `OrderRejectedEvent`, `OrderCancelledEvent`.
- **TraderRegistry** — 8 trader types on a 10ms tick thread. Handles fill notifications, portfolio updates, and market event spawning.
- **Portfolio** — per-trader balance, position, avg cost, unrealized PnL. Thread-safe with `std::mutex`.
- **Order types** — immutable `std::variant<LimitOrder, MarketOrder, StopLimitOrder, StopMarketOrder>`. Fills recorded in separate `Fill` structs (audit trail, not mutation).
- **Prices** — `uint64_t` fixed-point where 10000 = $1.00 (e.g. $64,200 = 642,000,000). Timestamps use `std::chrono::nanoseconds`.

### Trader Types (`include/traders/`)

| Trader | Behavior |
|--------|----------|
| `MarketMakerTrader` | GBM price walk, quotes bid+ask each tick with fixed half-spread |
| `MomentumTrader` | 10-tick lookback, buys/sells on 0.5% momentum signal |
| `MeanReversionTrader` | 20-tick SMA, trades 2% deviation from mean |
| `TWAPTrader` | Splits large order into equal market-order slices over N ticks |
| `TrendFollowerTrader` | Dual MA crossover (5/20 tick), flips position on signal |
| `RandomLimitTrader` | Poisson arrival, normal price offset from last trade |
| `RandomMarketTrader` | Poisson arrival, random side market orders |
| `PanicTrader` | Spawned by market events only; aggressively dumps fixed qty then goes dormant |

### Market Events (`include/market_events.h`)

Triggered via `Exchange::trigger_event()` or the API. Each spawns/modifies traders for `duration_ticks`:
- `FlashCrash` — suspends market makers, spawns 4 panic sellers
- `BullRun` — spawns 3 aggressive buyers
- `LiquiditySqueeze` — suspends all random limit traders
- `MeanReversionTrap` — spawns 2 aggressive buyers (momentum spike)

### Python Bindings (`src/python_bindings.cpp`)

Exposes enums (`Side`, `TimeInForce`, `MarketEventType`), structs (`BookSnapshot`, `Fill`, `SystemMetrics`, `PortfolioSnapshot`, `TraderMetrics`, `TraderInfo`, `MarketEventInfo`), and the full `Exchange` class. The compiled `.so` is output to `backend/`.

Key methods on `Exchange`:
- `start(seed_price)` / `stop()` / `is_running()`
- `submit_limit_order(trader_id, is_buy, price, qty, tif="GTC")` → `order_id`
- `submit_market_order(trader_id, is_buy, qty)` → `order_id`
- `cancel_order(order_id)`
- `book_snapshot()` → `BookSnapshot`
- `recent_trades(limit=50)` → `[Fill]`
- `metrics()` → `SystemMetrics`
- `create_portfolio(balance)` → `trader_id`
- `portfolio_snapshot(trader_id)` → `PortfolioSnapshot`
- `add_market_maker(name, balance, seed_price)` → `trader_id`
- `add_momentum_trader / add_mean_reversion_trader / add_twap_trader / add_trend_follower / add_random_limit_trader / add_random_market_trader`
- `start_trader(id)` / `stop_trader(id)` / `remove_trader(id)`
- `trigger_event(MarketEventType, duration_ticks=30)`
- `on_fill_callback(fn)` / `on_book_update_callback(fn)` — Python callbacks called with GIL

### Backend (`backend/main.py`)

FastAPI app wrapping `Exchange` as a singleton. Seeded with live BTC price from CoinGecko at startup. Key endpoint groups:
- `POST/GET /engine/*` — start/stop/status
- `GET /metrics`, `/book_snapshot`, `/recent_trades`
- `GET|POST|PUT|DELETE /traders` — automated trader CRUD + toggle
- `POST /events/trigger` — trigger a market event
- `GET /portfolio/{trader_id}`, `POST /portfolio/reset`
- `WS /ws` — pushes `book_update` and `trade` messages

### Frontend (`frontend/trading_exchange_frontend/`)

React 19 + TypeScript + Vite + Tailwind (dark mode) + Recharts. WIP — being redesigned. Target layout: book-dominant (order book center), observe-first, single BTC symbol.

## Include Path Convention

Two CMake targets with different roots — important when adding files:
- `core_lib` / `trading_engine_py` have `${PROJECT_SOURCE_DIR}/include` in their include path → source files in `src/` and `include/` use `#include "order.h"`, `#include "traders/momentum.h"` etc.
- `tests` target has `${PROJECT_SOURCE_DIR}` (repo root) → test files use `#include "include/order.h"`, `#include "include/traders/momentum.h"` etc.

## TODO

- Backend (`backend/main.py`) needs to be rewritten for the new `Exchange` API — currently uses the old `TradingSystem` interface.
- Frontend rewrite pending (wireframe designed, Figma/Stitch mockup to follow).
- WebSocket `/ws` endpoint not yet implemented in backend.
- `orders_processed` in `SystemMetrics` only counts orders submitted via `Exchange::submit_order()`, not trader-registry-submitted orders.
- No authentication system.
