# Readability Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize Talat's C++ core into subsystem directories, add header-level docs, close a real FOK correctness bug plus its test gap, expose Stop/FOK orders in the TUI, and split the README into a skimmable quickstart + a separate technical deep-dive with AI-generated writing tells removed.

**Architecture:** Three sequential directory-move tasks (`core/` → `engine/` → `market/`, each independently buildable), then a CLAUDE.md sync, then three independent feature/content tasks (FOK fix, TUI parser extraction, README/DESIGN split). Every task ends with `make build` + `make test` green.

**Tech Stack:** C++20, CMake 3.14+, Boost, GoogleTest, ftxui v5 (unchanged — no new dependencies in this phase).

## Global Constraints

- Every task must leave `make build` and `make test` passing before commit — no task lands in a broken state.
- Quote-includes are written relative to the `include/` root everywhere (e.g. `#include "engine/exchange.h"`), never relative to the including file's own directory. This means once a reference is fixed to point at a header's new location, it never needs touching again even if the *including* file itself moves later in a subsequent task.
- Use `git mv` for file relocations (preserves history), not delete+recreate.
- No behavior changes except the explicit FOK fix in Task 5 and the new TUI commands in Task 6 — the directory-move tasks (1-3) and CLAUDE.md sync (4) are pure reorganization.
- Header doc comments are 2-5 line plain `//` blocks (matches existing codebase style — no Doxygen), placed directly above the class declaration.

---

## Task 1: Move `order.h` into `include/core/`

**Files:**
- Create: `include/core/order.h` (moved from `include/order.h`)
- Modify: every file listed in the reference table below

**Interfaces:**
- Produces: `core/order.h` as the new canonical include path for `Order`, `LimitOrder`, `MarketOrder`, `StopLimitOrder`, `StopMarketOrder`, `Side`, `TimeInForce`, `Fill`, and the `OrderId`/`TraderId`/`Price`/`Quantity`/`Timestamp` typedefs. All later tasks reference this as `"core/order.h"` (non-test) or `"include/core/order.h"` (tests).

- [ ] **Step 1: Move the file**

```bash
mkdir -p include/core
git mv include/order.h include/core/order.h
```

- [ ] **Step 2: Add a one-line file purpose comment**

At the top of `include/core/order.h`, immediately after the existing `#pragma once` and includes, above the `using OrderId = ...` block, add:

```cpp
// Core order/fill data types shared by the engine and market layers.
// Pure data — no logic, no dependencies on any other subsystem.
```

- [ ] **Step 3: Fix every reference to `order.h`**

Apply this exact substitution in each file (old → new):

| File | Old include | New include |
|---|---|---|
| `include/exchange_events.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/tui/tui.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/trader.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/portfolio.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/order_matcher.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/exchange.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/order_book.h` | `#include "order.h"` | `#include "core/order.h"` |
| `include/latent_price.h` | `#include "order.h"` | `#include "core/order.h"` |
| `src/tui/tui.cpp` | `#include "order.h"` | `#include "core/order.h"` |
| `tests/cpp/test_event_bus.cpp` | `#include "include/order.h"` | `#include "include/core/order.h"` |
| `tests/cpp/performanceTest.cpp` | `#include "include/order.h"` | `#include "include/core/order.h"` |
| `tests/cpp/test_order_types.cpp` | `#include "include/order.h"` | `#include "include/core/order.h"` |

- [ ] **Step 4: Build and test**

```bash
make build
make test
```
Expected: both succeed, same test count as before this change.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: move order.h into include/core/"
```

---

## Task 2: Move engine files into `include/engine/` and `src/engine/`

**Files:**
- Move: `include/order_book.h` → `include/engine/order_book.h`
- Move: `src/order_book.cpp` → `src/engine/order_book.cpp`
- Move: `include/exchange_events.h` → `include/engine/exchange_events.h`
- Move: `include/order_matcher.h` → `include/engine/order_matcher.h`
- Move: `src/order_matcher.cpp` → `src/engine/order_matcher.cpp`
- Move: `include/event_bus.h` → `include/engine/event_bus.h`
- Move: `src/event_bus.cpp` → `src/engine/event_bus.cpp`
- Move: `include/exchange.h` → `include/engine/exchange.h`
- Move: `src/exchange.cpp` → `src/engine/exchange.cpp`
- Move: `include/portfolio.h` → `include/engine/portfolio.h`
- Move: `src/portfolio.cpp` → `src/engine/portfolio.cpp`
- Move: `include/stats_tracker.h` → `include/engine/stats_tracker.h`
- Modify: `CMakeLists.txt`, every file in the reference table below

**Interfaces:**
- Consumes: `"core/order.h"` (from Task 1, already correct everywhere, needs no changes here).
- Produces: `engine/{order_book,exchange_events,order_matcher,event_bus,exchange,portfolio,stats_tracker}.h` as canonical paths for `OrderBook`, `BookSnapshot`, `BookWalkCallback`, `FillEvent`/`BookUpdateEvent`/`OrderAcceptedEvent`/`OrderRejectedEvent`/`OrderCancelledEvent`, `OrderMatcher`, `EventBus`/`SubscriptionToken`, `Exchange`/`SystemMetrics`/`PortfolioSnapshot`, `Portfolio`, `StatsTracker`.

- [ ] **Step 1: Move the files**

```bash
mkdir -p include/engine src/engine
git mv include/order_book.h       include/engine/order_book.h
git mv src/order_book.cpp         src/engine/order_book.cpp
git mv include/exchange_events.h  include/engine/exchange_events.h
git mv include/order_matcher.h    include/engine/order_matcher.h
git mv src/order_matcher.cpp      src/engine/order_matcher.cpp
git mv include/event_bus.h        include/engine/event_bus.h
git mv src/event_bus.cpp          src/engine/event_bus.cpp
git mv include/exchange.h         include/engine/exchange.h
git mv src/exchange.cpp           src/engine/exchange.cpp
git mv include/portfolio.h        include/engine/portfolio.h
git mv src/portfolio.cpp          src/engine/portfolio.cpp
git mv include/stats_tracker.h    include/engine/stats_tracker.h
```

- [ ] **Step 2: Add header doc comments**

Above `class OrderBook {` in `include/engine/order_book.h`:
```cpp
// Price-time-priority limit order book. Two boost::container::flat_map
// levels (bids, asks) for cache-friendly sorted access. A shared_mutex
// lets readers (TUI snapshots, trader signal checks) run concurrently;
// the matcher thread takes an exclusive lock only while mutating.
```

Above `class OrderMatcher {` in `include/engine/order_matcher.h`:
```cpp
// Processes every order/cancel on one dedicated worker thread, fed by a
// lock-free moodycamel::ConcurrentQueue so callers on any thread never
// block on submit(). All book mutation happens without locks on this
// one thread; concurrent readers rely on OrderBook's shared_mutex.
```

Above `class EventBus {` in `include/engine/event_bus.h`:
```cpp
// Type-safe pub/sub keyed by std::type_index with handlers stored as
// std::any. Publish takes a shared lock so multiple publishers never
// block each other; subscribe/unsubscribe take an exclusive lock since
// those are the only operations that mutate the handler map.
```

Above `class Exchange {` in `include/engine/exchange.h`:
```cpp
// Top-level orchestrator: owns the OrderMatcher, TraderRegistry,
// EventBus, and the per-trader Portfolio map. This is the single entry
// point a frontend talks to — submit_order/cancel_order/snapshots all
// flow through here instead of touching subsystems directly.
```

Above `class Portfolio {` in `include/engine/portfolio.h`:
```cpp
// Per-trader balance/position/avg-cost accounting, updated on every
// fill via apply_fill. Thread-safe with its own mutex since fills can
// arrive from the matcher thread while a reader takes a snapshot.
```

Above `class StatsTracker {` in `include/engine/stats_tracker.h`:
```cpp
// Header-only rolling latency/throughput tracker. Subscribes to
// OrderAcceptedEvent and keeps a 5-second sliding window of
// (timestamp, latency) samples; snapshot() derives p50/p99 latency and
// orders/sec from whatever's still in the window.
```

- [ ] **Step 3: Fix self-includes in the moved `.cpp` files**

| File | Old include | New include |
|---|---|---|
| `src/engine/order_book.cpp` | `#include "order_book.h"` | `#include "engine/order_book.h"` |
| `src/engine/order_matcher.cpp` | `#include "order_matcher.h"` | `#include "engine/order_matcher.h"` |
| `src/engine/order_matcher.cpp` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `src/engine/event_bus.cpp` | `#include "event_bus.h"` | `#include "engine/event_bus.h"` |
| `src/engine/exchange.cpp` | `#include "exchange.h"` | `#include "engine/exchange.h"` |
| `src/engine/exchange.cpp` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `src/engine/portfolio.cpp` | `#include "portfolio.h"` | `#include "engine/portfolio.h"` |

- [ ] **Step 4: Fix internal cross-includes among the moved headers**

| File | Old include | New include |
|---|---|---|
| `include/engine/exchange_events.h` | `#include "order_book.h"` | `#include "engine/order_book.h"` |
| `include/engine/order_matcher.h` | `#include "order_book.h"` | `#include "engine/order_book.h"` |
| `include/engine/order_matcher.h` | `#include "event_bus.h"` | `#include "engine/event_bus.h"` |
| `include/engine/exchange.h` | `#include "order_book.h"` | `#include "engine/order_book.h"` |
| `include/engine/exchange.h` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `include/engine/exchange.h` | `#include "order_matcher.h"` | `#include "engine/order_matcher.h"` |
| `include/engine/exchange.h` | `#include "event_bus.h"` | `#include "engine/event_bus.h"` |
| `include/engine/exchange.h` | `#include "portfolio.h"` | `#include "engine/portfolio.h"` |
| `include/engine/stats_tracker.h` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `include/engine/stats_tracker.h` | `#include "event_bus.h"` | `#include "engine/event_bus.h"` |

- [ ] **Step 5: Fix references from files outside `engine/` (not yet moved or never moving)**

| File | Old include | New include |
|---|---|---|
| `include/trader_registry.h` | `#include "order_matcher.h"` | `#include "engine/order_matcher.h"` |
| `include/trader_registry.h` | `#include "event_bus.h"` | `#include "engine/event_bus.h"` |
| `include/trader.h` | `#include "portfolio.h"` | `#include "engine/portfolio.h"` |
| `src/trader_registry.cpp` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `include/tui/tui.h` | `#include "exchange.h"` | `#include "engine/exchange.h"` |
| `include/tui/tui.h` | `#include "stats_tracker.h"` | `#include "engine/stats_tracker.h"` |
| `src/tui/tui.cpp` | `#include "exchange_events.h"` | `#include "engine/exchange_events.h"` |
| `src/main.cpp` | `#include "exchange.h"` | `#include "engine/exchange.h"` |

- [ ] **Step 6: Fix test references**

| File | Old include | New include |
|---|---|---|
| `tests/cpp/test_order_book.cpp` | `#include "include/order_book.h"` | `#include "include/engine/order_book.h"` |
| `tests/cpp/test_order_matcher.cpp` | `#include "include/order_matcher.h"` | `#include "include/engine/order_matcher.h"` |
| `tests/cpp/test_order_matcher.cpp` | `#include "include/exchange_events.h"` | `#include "include/engine/exchange_events.h"` |
| `tests/cpp/test_trader_registry.cpp` | `#include "include/exchange_events.h"` | `#include "include/engine/exchange_events.h"` |
| `tests/cpp/test_trader_registry.cpp` | `#include "include/order_matcher.h"` | `#include "include/engine/order_matcher.h"` |
| `tests/cpp/test_trader_registry.cpp` | `#include "include/event_bus.h"` | `#include "include/engine/event_bus.h"` |
| `tests/cpp/test_event_bus.cpp` | `#include "include/event_bus.h"` | `#include "include/engine/event_bus.h"` |
| `tests/cpp/test_exchange.cpp` | `#include "include/exchange.h"` | `#include "include/engine/exchange.h"` |
| `tests/cpp/test_portfolio.cpp` | `#include "include/portfolio.h"` | `#include "include/engine/portfolio.h"` |
| `tests/cpp/performanceTest.cpp` | `#include "include/exchange_events.h"` | `#include "include/engine/exchange_events.h"` |
| `tests/cpp/performanceTest.cpp` | `#include "include/event_bus.h"` | `#include "include/engine/event_bus.h"` |
| `tests/cpp/performanceTest.cpp` | `#include "include/exchange.h"` | `#include "include/engine/exchange.h"` |

- [ ] **Step 7: Update `CMakeLists.txt`**

In the `CORE_SOURCES` list, change:
```cmake
    src/event_bus.cpp
    src/exchange.cpp
    src/order_book.cpp
    src/order_matcher.cpp
    src/portfolio.cpp
```
to:
```cmake
    src/engine/event_bus.cpp
    src/engine/exchange.cpp
    src/engine/order_book.cpp
    src/engine/order_matcher.cpp
    src/engine/portfolio.cpp
```
(leave `src/trader.cpp`, `src/trader_registry.cpp`, `src/traders/*.cpp` untouched — Task 3 handles those.)

- [ ] **Step 8: Build and test**

```bash
make build
make test
```
Expected: both succeed.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "refactor: move engine subsystem into include/engine/ and src/engine/"
```

---

## Task 3: Move market files into `include/market/` and `src/market/`

**Files:**
- Move: `include/latent_price.h` → `include/market/latent_price.h`
- Move: `include/trader.h` → `include/market/trader.h`
- Move: `src/trader.cpp` → `src/market/trader.cpp`
- Move: `include/trader_registry.h` → `include/market/trader_registry.h`
- Move: `src/trader_registry.cpp` → `src/market/trader_registry.cpp`
- Move: `include/traders/market_maker.h` → `include/market/traders/market_maker.h`
- Move: `src/traders/market_maker.cpp` → `src/market/traders/market_maker.cpp`
- Move: `include/traders/informed_trader.h` → `include/market/traders/informed_trader.h`
- Move: `src/traders/informed_trader.cpp` → `src/market/traders/informed_trader.cpp`
- Move: `include/traders/noise_trader.h` → `include/market/traders/noise_trader.h`
- Move: `src/traders/noise_trader.cpp` → `src/market/traders/noise_trader.cpp`
- Modify: `CMakeLists.txt`, `CLAUDE.md` build note, every file in the reference table below

**Interfaces:**
- Consumes: `"engine/portfolio.h"`, `"engine/order_matcher.h"`, `"engine/event_bus.h"` (from Task 2, already correct, no changes needed here).
- Produces: `market/{latent_price,trader,trader_registry}.h` and `market/traders/{market_maker,informed_trader,noise_trader}.h` as canonical paths for `LatentPrice`, `Trader`, `TraderRegistry`, `MarketMaker`, `InformedTrader`, `NoiseTrader`.

- [ ] **Step 1: Move the files**

```bash
mkdir -p include/market/traders src/market/traders
git mv include/latent_price.h              include/market/latent_price.h
git mv include/trader.h                    include/market/trader.h
git mv src/trader.cpp                      src/market/trader.cpp
git mv include/trader_registry.h           include/market/trader_registry.h
git mv src/trader_registry.cpp             src/market/trader_registry.cpp
git mv include/traders/market_maker.h      include/market/traders/market_maker.h
git mv src/traders/market_maker.cpp        src/market/traders/market_maker.cpp
git mv include/traders/informed_trader.h   include/market/traders/informed_trader.h
git mv src/traders/informed_trader.cpp     src/market/traders/informed_trader.cpp
git mv include/traders/noise_trader.h      include/market/traders/noise_trader.h
git mv src/traders/noise_trader.cpp        src/market/traders/noise_trader.cpp
rmdir include/traders src/traders
```

- [ ] **Step 2: Add header doc comments**

Above `class TraderRegistry {` in `include/market/trader_registry.h`:
```cpp
// Owns the LatentPrice GBM process and drives all automated traders
// (MarketMaker, InformedTrader, NoiseTrader) on one configurable tick
// thread. Each tick advances the fair value once, then lets every
// active trader react to it — this is what produces price action.
```

Above `class LatentPrice {` in `include/market/latent_price.h`:
```cpp
// Header-only zero-drift Geometric Brownian Motion process standing in
// for a theoretical fair value. Stored as an atomic Price so any
// trader thread can read it lock-free; sigma controls per-tick
// volatility.
```

Above `class Trader {` in `include/market/trader.h`:
```cpp
// Base class for all automated trader types. Holds the submit/cancel
// hooks into the matcher plus the static order-ID allocator every
// trader (and the TUI, for user orders) draws from.
```

Above `class MarketMaker : public Trader {` in `include/market/traders/market_maker.h`:
```cpp
// Quotes bid+ask symmetrically around the latent fair value every
// tick, cancelling and requoting each time. Implements Glosten-Milgrom
// adverse selection: widens its spread up to 3x when its own fill rate
// over a rolling window climbs too high, a sign of informed flow.
```

Above `class InformedTrader : public Trader {` in `include/market/traders/informed_trader.h`:
```cpp
// Simulates a trader with a noisy signal about true value. Submits an
// IOC limit order at the signal price whenever it diverges from the
// last trade price beyond a threshold — directional pressure that
// drives price discovery. Never rests an order.
```

Above `class NoiseTrader : public Trader {` in `include/market/traders/noise_trader.h`:
```cpp
// Uninformed liquidity: Poisson-arrival orders, random side,
// log-normal size, mostly limit near mid with some market orders
// mixed in. Provides the background flow that makes the book behave
// like a real one instead of two rational agents talking past each
// other.
```

- [ ] **Step 3: Fix self-includes in the moved `.cpp` files**

| File | Old include | New include |
|---|---|---|
| `src/market/trader.cpp` | `#include "trader.h"` | `#include "market/trader.h"` |
| `src/market/trader_registry.cpp` | `#include "trader_registry.h"` | `#include "market/trader_registry.h"` |
| `src/market/traders/market_maker.cpp` | `#include "traders/market_maker.h"` | `#include "market/traders/market_maker.h"` |
| `src/market/traders/informed_trader.cpp` | `#include "traders/informed_trader.h"` | `#include "market/traders/informed_trader.h"` |
| `src/market/traders/noise_trader.cpp` | `#include "traders/noise_trader.h"` | `#include "market/traders/noise_trader.h"` |

- [ ] **Step 4: Fix internal cross-includes among the moved headers**

| File | Old include | New include |
|---|---|---|
| `include/market/trader_registry.h` | `#include "latent_price.h"` | `#include "market/latent_price.h"` |
| `include/market/trader_registry.h` | `#include "trader.h"` | `#include "market/trader.h"` |
| `include/market/trader_registry.h` | `#include "traders/market_maker.h"` | `#include "market/traders/market_maker.h"` |
| `include/market/trader_registry.h` | `#include "traders/informed_trader.h"` | `#include "market/traders/informed_trader.h"` |
| `include/market/trader_registry.h` | `#include "traders/noise_trader.h"` | `#include "market/traders/noise_trader.h"` |
| `include/market/traders/market_maker.h` | `#include "trader.h"` | `#include "market/trader.h"` |
| `include/market/traders/market_maker.h` | `#include "latent_price.h"` | `#include "market/latent_price.h"` |
| `include/market/traders/informed_trader.h` | `#include "trader.h"` | `#include "market/trader.h"` |
| `include/market/traders/informed_trader.h` | `#include "latent_price.h"` | `#include "market/latent_price.h"` |
| `include/market/traders/noise_trader.h` | `#include "trader.h"` | `#include "market/trader.h"` |
| `src/market/trader_registry.cpp` | `#include "trader_registry.h"` (self, already listed Step 3) | — |

- [ ] **Step 5: Fix references from files outside `market/`**

| File | Old include | New include |
|---|---|---|
| `include/tui/tui.h` | `#include "trader_registry.h"` | `#include "market/trader_registry.h"` |
| `src/tui/tui.cpp` | `#include "trader.h"` | `#include "market/trader.h"` |
| `include/engine/exchange.h` | `#include "trader_registry.h"` | `#include "market/trader_registry.h"` |

- [ ] **Step 6: Fix test references**

| File | Old include | New include |
|---|---|---|
| `tests/cpp/test_new_traders.cpp` | `#include "include/latent_price.h"` | `#include "include/market/latent_price.h"` |
| `tests/cpp/test_new_traders.cpp` | `#include "include/traders/market_maker.h"` | `#include "include/market/traders/market_maker.h"` |
| `tests/cpp/test_new_traders.cpp` | `#include "include/traders/informed_trader.h"` | `#include "include/market/traders/informed_trader.h"` |
| `tests/cpp/test_new_traders.cpp` (line ~178) | `#include "include/traders/noise_trader.h"` | `#include "include/market/traders/noise_trader.h"` |
| `tests/cpp/test_latent_price.cpp` | `#include "include/latent_price.h"` | `#include "include/market/latent_price.h"` |
| `tests/cpp/test_trader_registry.cpp` | `#include "include/trader_registry.h"` | `#include "include/market/trader_registry.h"` |

- [ ] **Step 7: Update `CMakeLists.txt`**

In the `CORE_SOURCES` list, change:
```cmake
    src/trader.cpp
    src/trader_registry.cpp
    src/traders/market_maker.cpp
    src/traders/informed_trader.cpp
    src/traders/noise_trader.cpp
```
to:
```cmake
    src/market/trader.cpp
    src/market/trader_registry.cpp
    src/market/traders/market_maker.cpp
    src/market/traders/informed_trader.cpp
    src/market/traders/noise_trader.cpp
```

- [ ] **Step 8: Build and test**

```bash
make build
make test
```
Expected: both succeed.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "refactor: move market subsystem into include/market/ and src/market/"
```

---

## Task 4: Update `CLAUDE.md` for the new layout

**Files:**
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: final directory layout from Tasks 1-3 (no new code).

- [ ] **Step 1: Replace the "Include Path Convention" section**

Find this block in `CLAUDE.md`:
```markdown
### Include Path Convention

Two CMake targets with different roots:
- `core_lib` / `trading_engine` have `${PROJECT_SOURCE_DIR}/include` in their include path → source files use `#include "order.h"`, `#include "traders/market_maker.h"` etc.
- `tests` target has `${PROJECT_SOURCE_DIR}` (repo root) → test files use `#include "include/order.h"`, `#include "include/traders/market_maker.h"` etc.
```

Replace with:
```markdown
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
```

- [ ] **Step 2: Update the "C++ Core" file references**

In the `### C++ Core (`src/`, `include/`)` section, update each bolded file reference to its new path, e.g. `**Exchange** (`include/exchange.h`)` → `**Exchange** (`include/engine/exchange.h`)`. Apply the same pattern for `OrderMatcher` (`include/engine/order_matcher.h`), `OrderBook` (`include/engine/order_book.h`), `EventBus` (`include/engine/event_bus.h`), `TraderRegistry` (`include/market/trader_registry.h`), `LatentPrice` (`include/market/latent_price.h`), `StatsTracker` (`include/engine/stats_tracker.h`).

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: sync CLAUDE.md with engine/market/core directory layout"
```

---

## Task 5: Fix FOK correctness bug and add coverage

**Files:**
- Modify: `src/engine/order_matcher.cpp` (the `try_match_limit` function)
- Modify: `tests/cpp/test_order_matcher.cpp`
- Delete: `tests/cpp/placeholder_test.cpp`

**Interfaces:**
- Consumes: `OrderMatcher`, `EventBus`, `OrderRejectedEvent`, `FillEvent` (all unchanged signatures from Task 2's move).
- Produces: no new public interface — `try_match_limit` behavior changes for `TimeInForce::FOK` only (now genuinely all-or-nothing instead of behaving like IOC).

- [ ] **Step 1: Write the failing tests**

Add to `tests/cpp/test_order_matcher.cpp`, after the existing `SelfMatchPrevented` test:

```cpp
TEST(OrderMatcher, FOKFullyFillsWhenLiquiditySufficient) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });

    submit_and_wait(matcher, make_limit(1, 10, Side::Sell, 100, 50));
    submit_and_wait(matcher, make_limit(2, 11, Side::Buy, 100, 50, TimeInForce::FOK));

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_qty, 50ULL);
    EXPECT_FALSE(matcher.book().best_ask().has_value());

    matcher.stop();
}

TEST(OrderMatcher, FOKRejectsWithZeroFillsWhenLiquidityInsufficient) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    std::vector<Fill> fills;
    bool rejected = false;
    bus.subscribe<FillEvent>([&](const FillEvent& e) { fills.push_back(e.fill); });
    bus.subscribe<OrderRejectedEvent>([&](const OrderRejectedEvent& e) {
        if (e.order_id == 2) rejected = true;
    });

    // Only 30 units resting; FOK taker wants 50 — must reject with zero fills.
    submit_and_wait(matcher, make_limit(1, 10, Side::Sell, 100, 30));
    submit_and_wait(matcher, make_limit(2, 11, Side::Buy, 100, 50, TimeInForce::FOK));

    EXPECT_TRUE(fills.empty());
    EXPECT_TRUE(rejected);
    // The resting sell order must be untouched — no partial execution.
    ASSERT_TRUE(matcher.book().best_ask().has_value());
    EXPECT_EQ(*matcher.book().best_ask(), 100ULL);

    matcher.stop();
}

TEST(OrderMatcher, FOKRejectsWithNoLiquidityAtAll) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    bool rejected = false;
    bus.subscribe<OrderRejectedEvent>([&](const OrderRejectedEvent& e) {
        if (e.order_id == 1) rejected = true;
    });

    submit_and_wait(matcher, make_limit(1, 10, Side::Buy, 100, 50, TimeInForce::FOK));

    EXPECT_TRUE(rejected);
    EXPECT_FALSE(matcher.book().best_bid().has_value());

    matcher.stop();
}
```

- [ ] **Step 2: Run the new tests and confirm the second one fails**

```bash
make build
./build/tests --gtest_filter="OrderMatcher.FOK*"
```
Expected: `FOKFullyFillsWhenLiquiditySufficient` and `FOKRejectsWithNoLiquidityAtAll` pass (current code happens to handle these two correctly), but `FOKRejectsWithZeroFillsWhenLiquidityInsufficient` FAILS — the current implementation partially fills 30 units instead of rejecting cleanly. This confirms the bug.

- [ ] **Step 3: Fix `try_match_limit`**

In `src/engine/order_matcher.cpp`, find the start of `try_match_limit`:
```cpp
void OrderMatcher::try_match_limit(const LimitOrder& taker, std::chrono::steady_clock::time_point dequeue_tp) {
    Quantity remaining = taker.qty;
```

Insert a liquidity precheck immediately before `Quantity remaining = taker.qty;`:
```cpp
void OrderMatcher::try_match_limit(const LimitOrder& taker, std::chrono::steady_clock::time_point dequeue_tp) {
    if (taker.tif == TimeInForce::FOK) {
        // All-or-nothing: sum available quantity at qualifying price levels
        // before executing anything. Doesn't exclude the taker's own resting
        // orders from the count (self-match prevention would still block
        // those fills) — an extremely rare edge case where a trader is
        // resting on both sides at a qualifying price is not handled here.
        Quantity available = 0;
        auto liquidity_cb = [&](Price level_price, Quantity level_qty, const std::vector<OrderId>&) -> bool {
            bool price_ok = (taker.side == Side::Buy)
                ? (level_price <= taker.price)
                : (level_price >= taker.price);
            if (!price_ok) return true; // stop walking
            available += level_qty;
            return available >= taker.qty; // stop early once enough
        };
        if (taker.side == Side::Buy) book_.for_each_ask(liquidity_cb);
        else                          book_.for_each_bid(liquidity_cb);

        if (available < taker.qty) {
            bus_.publish(OrderRejectedEvent{taker.id, taker.trader_id, "insufficient_liquidity_fok"});
            return;
        }
    }

    Quantity remaining = taker.qty;
```

- [ ] **Step 4: Run the tests again and confirm all three pass**

```bash
make build
./build/tests --gtest_filter="OrderMatcher.FOK*"
```
Expected: all three PASS.

- [ ] **Step 5: Delete the dead placeholder test**

```bash
git rm tests/cpp/placeholder_test.cpp
```

- [ ] **Step 6: Run the full suite**

```bash
make test
```
Expected: all tests pass (one fewer suite than before, from the placeholder removal; three more test cases, from the new FOK tests).

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "fix: enforce true all-or-nothing FOK semantics in try_match_limit

FOK previously behaved identically to IOC: it emitted real fills while
walking the book and only decided afterward not to rest the remainder,
so a FOK order the book couldn't fully satisfy would partially execute
instead of rejecting cleanly. Added a liquidity precheck that rejects
with zero fills before any execution when available quantity at
qualifying price levels is less than the order's full quantity."
```

---

## Task 6: Extract testable order-command parser, add Stop/FOK TUI commands

**Files:**
- Create: `include/tui/order_command_parser.h`
- Create: `src/tui/order_command_parser.cpp`
- Create: `tests/cpp/test_order_command_parser.cpp`
- Modify: `src/tui/tui.cpp` (`handle_order_command`, `/help` text)
- Modify: `CMakeLists.txt` (add the new source to `CORE_SOURCES`)

**Interfaces:**
- Consumes: `core/order.h` types (`Order`, `LimitOrder`, `MarketOrder`, `StopLimitOrder`, `StopMarketOrder`, `Side`, `TimeInForce`), `market/trader.h`'s `Trader::alloc_order_id()`.
- Produces: `parse_order_command(const std::string& cmd, TraderId user_id, std::string& error) -> std::optional<ParsedOrderCommand>`, where `ParsedOrderCommand { Order order; std::string status; bool tracks_resting_price; }`. Consumed by `TUI::handle_order_command`.

- [ ] **Step 1: Write the failing tests**

Create `tests/cpp/test_order_command_parser.cpp`:
```cpp
#include <gtest/gtest.h>
#include "include/tui/order_command_parser.h"

TEST(OrderCommandParser, LimitBuy) {
    std::string error;
    auto result = parse_order_command("buy 5 @ 64000", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<LimitOrder>(result->order));
    auto lo = std::get<LimitOrder>(result->order);
    EXPECT_EQ(lo.side, Side::Buy);
    EXPECT_EQ(lo.qty, 5ULL);
    EXPECT_EQ(lo.price, 640000000ULL);
    EXPECT_EQ(lo.tif, TimeInForce::GTC);
    EXPECT_TRUE(result->tracks_resting_price);
}

TEST(OrderCommandParser, LimitSellFok) {
    std::string error;
    auto result = parse_order_command("sell 3 @ 100 fok", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<LimitOrder>(result->order));
    auto lo = std::get<LimitOrder>(result->order);
    EXPECT_EQ(lo.tif, TimeInForce::FOK);
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, MarketOrder) {
    std::string error;
    auto result = parse_order_command("buy 10", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<MarketOrder>(result->order));
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, StopMarket) {
    std::string error;
    auto result = parse_order_command("sell 2 stop 63000", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<StopMarketOrder>(result->order));
    auto so = std::get<StopMarketOrder>(result->order);
    EXPECT_EQ(so.stop_price, 630000000ULL);
    EXPECT_FALSE(result->tracks_resting_price);
}

TEST(OrderCommandParser, StopLimit) {
    std::string error;
    auto result = parse_order_command("buy 2 stop 63000 @ 63100", 1, error);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<StopLimitOrder>(result->order));
    auto so = std::get<StopLimitOrder>(result->order);
    EXPECT_EQ(so.stop_price, 630000000ULL);
    EXPECT_EQ(so.limit_price, 631000000ULL);
}

TEST(OrderCommandParser, RejectsUnknownVerb) {
    std::string error;
    auto result = parse_order_command("frobnicate 5", 1, error);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(error.empty());
}

TEST(OrderCommandParser, RejectsBadQty) {
    std::string error;
    auto result = parse_order_command("buy -5 @ 100", 1, error);
    EXPECT_FALSE(result.has_value());
}

TEST(OrderCommandParser, RejectsBadStopSyntax) {
    std::string error;
    auto result = parse_order_command("buy 5 stop", 1, error);
    EXPECT_FALSE(result.has_value());
}
```

- [ ] **Step 2: Create the header**

Create `include/tui/order_command_parser.h`:
```cpp
#pragma once
#include <optional>
#include <string>
#include "core/order.h"

struct ParsedOrderCommand {
    Order order;
    std::string status;          // human-readable confirmation for the status bar
    bool tracks_resting_price;   // true only for a GTC limit order (highlighted in the book)
};

// Parses a "buy"/"sell" order command: qty, optional "@ <price>" (with
// optional trailing "fok"), or optional "stop <price>" / "stop <price>
// @ <limit>". Returns std::nullopt and sets `error` to a usage string
// on bad syntax.
std::optional<ParsedOrderCommand> parse_order_command(const std::string& cmd, TraderId user_id, std::string& error);
```

- [ ] **Step 3: Create the implementation**

Create `src/tui/order_command_parser.cpp`:
```cpp
#include "tui/order_command_parser.h"
#include "market/trader.h"
#include <sstream>

std::optional<ParsedOrderCommand> parse_order_command(const std::string& cmd, TraderId user_id, std::string& error) {
    std::istringstream ss(cmd);
    std::string token;
    ss >> token;

    Side side;
    if (token == "buy")       side = Side::Buy;
    else if (token == "sell") side = Side::Sell;
    else { error = "Unknown command. Type /help."; return std::nullopt; }

    double qty_d = 0;
    if (!(ss >> qty_d) || qty_d <= 0) {
        error = "Usage: buy/sell <qty> [@ <price> [fok]] [stop <price> [@ <limit>]]";
        return std::nullopt;
    }
    Quantity qty = static_cast<Quantity>(qty_d);
    if (qty == 0) qty = 1;

    std::string modifier;
    ss >> modifier;

    OrderId oid = Trader::alloc_order_id();

    if (modifier == "@") {
        double price_d = 0;
        if (!(ss >> price_d) || price_d <= 0) {
            error = "Usage: buy <qty> @ <price> [fok]";
            return std::nullopt;
        }
        Price price = static_cast<Price>(price_d * 10000.0);

        std::string trailing;
        ss >> trailing;
        TimeInForce tif = TimeInForce::GTC;
        if (trailing == "fok") tif = TimeInForce::FOK;
        else if (!trailing.empty()) {
            error = "Usage: buy <qty> @ <price> [fok]";
            return std::nullopt;
        }

        ParsedOrderCommand result;
        result.order = LimitOrder{oid, user_id, side, price, qty, tif, {}};
        result.tracks_resting_price = (tif == TimeInForce::GTC);
        result.status = (side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                         " @ $" + std::to_string(static_cast<int>(price_d)) +
                         (tif == TimeInForce::FOK ? " FOK" : "") +
                         "  id=" + std::to_string(oid);
        return result;
    }

    if (modifier == "stop") {
        double stop_d = 0;
        if (!(ss >> stop_d) || stop_d <= 0) {
            error = "Usage: buy <qty> stop <stop_price> [@ <limit_price>]";
            return std::nullopt;
        }
        Price stop_price = static_cast<Price>(stop_d * 10000.0);

        std::string at;
        ss >> at;

        ParsedOrderCommand result;
        result.tracks_resting_price = false;

        if (at == "@") {
            double limit_d = 0;
            if (!(ss >> limit_d) || limit_d <= 0) {
                error = "Usage: buy <qty> stop <stop_price> @ <limit_price>";
                return std::nullopt;
            }
            Price limit_price = static_cast<Price>(limit_d * 10000.0);
            result.order = StopLimitOrder{oid, user_id, side, stop_price, limit_price, qty, {}};
            result.status = std::string(side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                             " stop $" + std::to_string(static_cast<int>(stop_d)) +
                             " limit $" + std::to_string(static_cast<int>(limit_d)) +
                             "  id=" + std::to_string(oid);
        } else if (at.empty()) {
            result.order = StopMarketOrder{oid, user_id, side, stop_price, qty, {}};
            result.status = std::string(side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                             " stop $" + std::to_string(static_cast<int>(stop_d)) +
                             "  id=" + std::to_string(oid);
        } else {
            error = "Usage: buy <qty> stop <stop_price> [@ <limit_price>]";
            return std::nullopt;
        }
        return result;
    }

    if (!modifier.empty()) {
        error = "Unknown order modifier '" + modifier + "'. Type /help.";
        return std::nullopt;
    }

    ParsedOrderCommand result;
    result.order = MarketOrder{oid, user_id, side, qty, TimeInForce::IOC, {}};
    result.tracks_resting_price = false;
    result.status = std::string("Market ") + (side == Side::Buy ? "buy " : "sell ") +
                     std::to_string(qty) + "  id=" + std::to_string(oid);
    return result;
}
```

- [ ] **Step 4: Add the new source to CMake and run the tests**

In `CMakeLists.txt`, add `src/tui/order_command_parser.cpp` to `CORE_SOURCES` (so both `trading_engine` and `tests` link it):
```cmake
set(CORE_SOURCES
    src/engine/event_bus.cpp
    src/engine/exchange.cpp
    src/engine/order_book.cpp
    src/engine/order_matcher.cpp
    src/engine/portfolio.cpp
    src/market/trader.cpp
    src/market/trader_registry.cpp
    src/market/traders/market_maker.cpp
    src/market/traders/informed_trader.cpp
    src/market/traders/noise_trader.cpp
    src/tui/order_command_parser.cpp
)
```

```bash
make build
./build/tests --gtest_filter="OrderCommandParser.*"
```
Expected: all pass.

- [ ] **Step 5: Wire the parser into `TUI::handle_order_command`**

In `src/tui/tui.cpp`, replace the entire body of `handle_order_command` (from `void TUI::handle_order_command(const std::string& cmd) {` through its closing `}`, currently lines 343-403) with:
```cpp
void TUI::handle_order_command(const std::string& cmd) {
    auto set_status = [this](const std::string& msg) {
        std::lock_guard lock(state_mutex_);
        status_message_ = msg;
    };

    std::istringstream peek(cmd);
    std::string token;
    peek >> token;

    if (token == "q" || token == "quit") {
        if (screen_) screen_->ExitLoopClosure()();
        return;
    }

    if (token == "cancel") {
        OrderId id = 0;
        if (!(peek >> id)) { set_status("Usage: cancel <order_id>"); return; }
        exchange_.cancel_order(id);
        set_status("Cancel requested for #" + std::to_string(id));
        return;
    }

    std::string error;
    auto parsed = parse_order_command(cmd, user_id_, error);
    if (!parsed) { set_status(error); return; }

    if (parsed->tracks_resting_price) {
        const auto& lo = std::get<LimitOrder>(parsed->order);
        std::lock_guard lock(state_mutex_);
        if (lo.side == Side::Buy) user_bid_prices_.insert(lo.price);
        else                      user_ask_prices_.insert(lo.price);
    }

    exchange_.submit_order(parsed->order);
    set_status(parsed->status);
}
```

Add `#include "tui/order_command_parser.h"` to the top of `src/tui/tui.cpp` alongside the other includes.

- [ ] **Step 6: Update the `/help` text**

In `src/tui/tui.cpp`, `handle_slash_command`, find the `ORDERS` block inside the `verb == "help"` branch and replace it with:
```cpp
            "ORDERS\n"
            "  buy <qty> @ <price>        limit buy (GTC)\n"
            "  sell <qty> @ <price>       limit sell (GTC)\n"
            "  buy <qty> @ <price> fok    limit buy, fill-or-kill\n"
            "  sell <qty> @ <price> fok   limit sell, fill-or-kill\n"
            "  buy <qty>                  market buy (IOC)\n"
            "  sell <qty>                 market sell (IOC)\n"
            "  buy <qty> stop <price>              stop-market buy\n"
            "  buy <qty> stop <price> @ <limit>    stop-limit buy\n"
            "  sell <qty> stop <price> [@ <limit>] stop-market/stop-limit sell\n"
            "  cancel <id>                cancel resting order\n"
            "  q / quit                   exit\n"
```

- [ ] **Step 7: Build, run the full test suite, and smoke-test manually**

```bash
make build
make test
```
Expected: all pass.

Then run `make run`, and manually try `buy 1 stop 60000`, `sell 1 @ 100 fok`, and `/help` to confirm the new commands are accepted and produce a sensible status message.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "feat: extract testable order_command_parser, expose stop/fok in TUI

Stop-Limit, Stop-Market, and FOK orders were fully implemented and
tested at the engine level but unreachable from the TUI command bar.
Extracted the buy/sell command parsing out of TUI::handle_order_command
into a free function (parse_order_command) that's unit-testable without
constructing a TUI/ftxui screen, and added stop/fok syntax to it."
```

---

## Task 7: Split README into quickstart + `docs/DESIGN.md` deep dive

**Files:**
- Modify: `README.md` (full rewrite)
- Create: `docs/DESIGN.md`

**Interfaces:**
- None (documentation only).

- [ ] **Step 1: Replace `README.md`**

Replace the entire contents of `README.md` with:

```markdown
# Talat

A high-performance simulated trading exchange in pure C++20 with a fullscreen terminal UI. *Talat* (ตลาด) means *market* in Thai.

\`\`\`
 TALAT   BTC   $63,847.12                              orders: 4823/s

┌── ORDER BOOK ──────────────┐┌── RECENT FILLS ────────────┐┌── STATS ──────────────────┐
│         ORDER BOOK         ││        RECENT FILLS        ││           STATS           │
│────────────────────────────││────────────────────────────││───────────────────────────│
│||||||||||   63852.14  108  ││ 63849.50    1  SELL        ││ p50 latency      108 us   │
│|||||||||    63851.03   84  ││ 63849.50    2   BUY        ││ p99 latency      144 us   │
│|||||||      63850.21   61  ││ 63848.75    1  SELL        ││ throughput      4823/s    │
│|||||        63849.88   42  ││ 63847.12    3   BUY        ││ book depth        24 lvls │
│|||          63849.50   22  ││ 63847.12    1   BUY        ││───────────────────────────│
│─────────── mid ────────────││ 63846.30    2  SELL        ││ ▲ +$124.50       TOTAL PnL│
│|||          63847.12   18  ││ 63845.00    1  SELL        ││───────────────────────────│
│|||||        63846.30   39  ││ 63844.20    4   BUY        ││ position             +3   │
│|||||||      63845.00   55  ││ 63843.10    1  SELL        ││ avg cost         $63,821  │
│|||||||||    63844.20   77  ││ 63843.10    2   BUY        ││ unreal PnL       +$78.36  │
│||||||||||   63843.10   99  ││ 63842.50    1  SELL        ││ real PnL         +$46.14  │
└────────────────────────────┘└────────────────────────────┘└───────────────────────────┘
 ❯ _
\`\`\`

## Requirements

- C++20 compiler (GCC 12+ or Clang 14+)
- CMake 3.14+
- Boost 1.74+

## Build & Run

\`\`\`bash
git clone --recurse-submodules https://github.com/RDG0818/TradingEngine.git
cd TradingEngine
make build    # cmake + compile (fetches ftxui on first run)
make run      # ./build/trading_engine
make run -- --seed 50000   # seed at $50,000
\`\`\`

\`\`\`bash
make test     # C++ test suite
make bench    # throughput, latency, contention benchmarks
\`\`\`

## Commands

Type commands in the bottom bar:

\`\`\`
buy <qty> @ <price>                  limit buy (GTC)
sell <qty> @ <price>                 limit sell (GTC)
buy <qty> @ <price> fok              limit buy, fill-or-kill
sell <qty> @ <price> fok             limit sell, fill-or-kill
buy <qty>                            market buy (IOC)
sell <qty>                           market sell (IOC)
buy <qty> stop <price>               stop-market buy
buy <qty> stop <price> @ <limit>     stop-limit buy
sell <qty> stop <price> [@ <limit>]  stop-market / stop-limit sell
cancel <order_id>                    cancel a resting order
q / quit                             exit
\`\`\`

Slash commands to control the simulation:

\`\`\`
/vol <0.0–1.0>               GBM volatility σ       (default: 0.0003)
/spread <dollars>            market maker half-spread (default: $2.00)
/speed <1–10>                tick speed, 1=2s 10=200ms (default: 1)
/pause / /resume              halt or restart all automated traders
/traders <mm|informed|noise> <n>   set trader count per type
/help                        show command reference
\`\`\`

## Architecture

\`\`\`
trading_engine (binary)
├── Exchange
│     ├── OrderMatcher    moodycamel lock-free queue, dedicated worker thread
│     ├── OrderBook       boost::container::flat_map, shared_mutex
│     ├── TraderRegistry  configurable tick thread, owns LatentPrice (GBM)
│     └── EventBus        type-safe pub/sub, std::type_index + std::any
└── TUI (ftxui v5)
      ├── Order Book panel   depth bars, 6 bid/ask levels, user orders highlighted
      ├── Recent Fills panel last 20 trades
      ├── Stats panel        p50/p99 latency, throughput, PnL breakdown
      └── Command bar        order entry + slash commands
\`\`\`

**Order types:** Limit (GTC/IOC/FOK), Market (IOC), Stop-Limit, Stop-Market

**Automated traders** run on a configurable tick thread and drive realistic price action:

| Trader | Behavior |
|---|---|
| \`MarketMaker\` | Quotes bid+ask ± GBM fair value. Widens spread up to 3× under adverse selection (Glosten-Milgrom). |
| \`InformedTrader\` | Noisy signal = latent × (1 + N(0,σ)). Submits IOC when signal diverges > 0.2% from last price. |
| \`NoiseTrader\` | Poisson arrivals (λ=0.7). 60% limit / 40% market split. Log-normal sizes. Random side. |

**Prices** are \`uint64_t\` fixed-point: \`10000 = $1.00\` (e.g. $64,200 → \`642000000\`).

## Performance

Benchmarked on an Intel i7-1255U (12-thread), Release build:

| Metric | Result |
|---|---|
| Submit throughput | 17M orders/sec (lock-free queue) |
| Match latency p50 | 108 µs |
| Match latency p99 | 144 µs |
| Match latency p99.9 | 215 µs |
| Write throughput (4 threads) | 6.7M orders/sec |
| Book snapshot reads (8 threads) | 940K snapshots/sec |

Design deep-dive, including matching engine internals, concurrency choices, and profiling results: see [docs/DESIGN.md](docs/DESIGN.md).

## License

MIT — see \`LICENSE\` for details.
```

(Note: the literal `\`\`\`` sequences above are markdown fence markers escaped for this plan document — write them as plain triple-backtick fences in the actual file.)

- [ ] **Step 2: Create `docs/DESIGN.md`**

Create `docs/DESIGN.md` with the following content:

```markdown
# Talat — Design Notes

Deep dive on the matching engine, concurrency model, and profiling results. See the [README](../README.md) for build/run instructions and the command reference.

## Matching Engine

OrderMatcher runs on one dedicated worker thread. All order and cancel commands land in a moodycamel ConcurrentQueue — a lock-free MPSC queue — wrapped in a `std::variant<SubmitCmd, CancelCmd>`, so any thread can enqueue without blocking, and the worker drains them in a tight bulk_dequeue loop. Since every book mutation happens on that one thread, there's no locking in the matching path, which is most of why raw submit throughput hits 17M/s.

Processing flow:
1. The dequeue timestamp is captured when the worker pulls the command off the queue, and passed through to `OrderAcceptedEvent` for latency measurement.
2. Limit orders call `try_match_limit`, which walks the opposite side of the book, filling against resting levels until the order is exhausted, the price no longer qualifies, or a TIF rule (IOC/FOK) forces a cancel.
3. Market orders walk the book with no price constraint at all.
4. Stop orders sit in `pending_stop_limits_` / `pending_stop_markets_` and get triggered by `check_stop_orders` after every fill.
5. If maker and taker share a trader ID, the fill is skipped — self-match prevention.
6. Partial fills are tracked per order in a separate `filled_qty_` map rather than mutating the order struct itself, so a resting `LimitOrder` never changes underneath any code holding a pointer to it.
7. FOK orders get an all-or-nothing liquidity check before any fill executes: available quantity at qualifying price levels is summed first, and the order rejects with zero fills if that total falls short of the requested quantity.

## Order Book

Bids and asks each live in a `boost::container::flat_map<Price, PriceLevel>` — a contiguous sorted array rather than a tree, so lookups are O(log n) with much better cache behavior than `std::map`. Both sides are stored ascending; bids are walked highest-first (`rbegin`→`rend`), asks lowest-first (`begin`→`end`).

Each `PriceLevel` tracks a running `total_qty` and a `std::list<OrderId>` for FIFO order within the level — a linked list because cancelling an order elsewhere in the queue needs O(1) removal via a stored iterator, which a vector can't give you. A separate `unordered_map<OrderId, LimitOrder>` holds the actual order structs so lookup by ID doesn't require scanning levels.

Concurrent access goes through a `shared_mutex`. Readers — TUI snapshots, informed traders checking best bid/ask — take a shared lock; the matcher thread takes an exclusive lock only while mutating. `snapshot()` copies the levels under lock and hands back a plain vector, so the TUI never holds the book lock while rendering.

## EventBus

A small type-erased pub/sub system. Subscriptions are `std::function<void(const EventType&)>` stored in an `unordered_map<std::type_index, vector<Handler>>`; publishing wraps the event in `std::any` and dispatches to every handler registered for that type. Publish takes a shared lock, so multiple publishers never block each other — only subscribe/unsubscribe take an exclusive lock, since those are the only operations that mutate the map. Each subscription returns a `SubscriptionToken` (a monotonic counter) for cleanup.

Events in play: `FillEvent` (carries the full `Fill`, including which side was the taker, for correct portfolio attribution), `BookUpdateEvent` (fires on any book mutation, triggers a TUI re-render), `OrderAcceptedEvent` (carries the dequeue timestamp for `StatsTracker`), and `OrderRejectedEvent` / `OrderCancelledEvent`.

## Latent Price (GBM)

`LatentPrice` models a theoretical fair value with zero-drift Geometric Brownian Motion. Each tick advances the log-price by σ·Z, Z ~ N(0,1), then converts back to fixed-point:

\`\`\`
log_price += σ * Z
price = exp(log_price) * 10000
\`\`\`

The price lives in a `std::atomic<Price>`, so any trader thread can read it lock-free — writes use release ordering, reads use acquire, which is enough to guarantee a reader never sees a torn or stale value without needing a full lock. Default σ is 0.0003 (about 0.03% per tick), adjustable at runtime via `/vol`.

## Automated Traders

All three trader types extend `Trader` and are driven by `TraderRegistry` on a single configurable tick thread (200ms by default). Each tick advances the GBM once, then gives every active trader a chance to submit or cancel through the matcher.

MarketMaker quotes a bid and ask around the GBM price, cancelling and requoting every tick. It tracks its own fill rate over a 20-tick window, and when that rate climbs past 30% — a sign informed traders are picking it off — it widens its half-spread by up to 3×. That's the Glosten-Milgrom insight in miniature: a market maker that's getting run over by informed flow protects itself by quoting wider.

InformedTrader carries a noisy signal about the true price: `signal = latent × (1 + N(0, σ))`. When that signal diverges from the last trade price by more than 0.2%, it fires an IOC limit order at the signal price. IOC means it either fills right away or disappears — it never rests, so it's pure directional pressure, not liquidity.

NoiseTrader is uninformed flow: Poisson arrivals (λ=0.7 per tick), random side, log-normal size, split 60/40 between limit orders near mid and market orders. It's what keeps the book populated and the tape noisy enough to look like a real market instead of two rational agents talking past each other.

## Portfolio & PnL

Each portfolio tracks balance, position, and running cost basis. On every fill, `apply_fill` runs with the correct side — takers get `Fill::taker_side` directly, makers get the opposite side, since the fill event only records the taker's side explicitly.

Unrealized PnL is `position × (current_price − avg_cost)`. Realized PnL is cash flow since the start (`balance − starting_balance`) plus the at-cost value of whatever's still open, which nets out the unrealized component. Total PnL is the same cash-flow figure but marked at the current price instead of cost — realized and unrealized added back together.

## StatsTracker

Subscribes to `OrderAcceptedEvent` and keeps a rolling 5-second deque of `(timestamp, latency)` samples. `snapshot()` filters to the window, sorts, and returns p50/p99 plus orders/sec. The latency measured here runs from dequeue time — when the matcher worker pulls the command off the queue — to when `OrderAcceptedEvent` publishes, so it captures queue drain and book insertion, not round-trip time back to whoever submitted the order.

## Fixed-Point Arithmetic

Prices are `uint64_t` with an implicit scale of 10,000 units per dollar — $64,200.50 is stored as `642,005,000`. This sidesteps floating-point rounding error in financial math entirely; the TUI and command parser convert to and from `double` only at the point where a human types or reads a number.

## Profiling Analysis

Profiled on an Intel i7-1255U (hybrid P-core/E-core, 12 threads), Release build.

Sanitizers are clean across the board: ASan+UBSan catch heap/stack overflows, use-after-free, and undefined behavior; TSan checks for data races across every concurrent path (matcher thread, trader registry, EventBus, shared_mutex). Both pass with no findings.

`perf stat`'s key counters: IPC sits around 0.55–0.61 on both core types, meaning the pipeline is roughly a quarter utilized — this workload is memory-latency-bound, not compute-bound. Cache miss rate runs 25–29%, which tracks with the `flat_map`/`unordered_map` working set exceeding L1 under load. Futex time totals 0.638ms — lock contention is negligible. Wall-clock sys time comes out to 34%, which looked concerning until a syscall trace (`strace -c`) showed it's almost entirely `clock_nanosleep` (the trader registry's tick thread sleeping between ticks) and `sched_yield` (the matcher yielding when the queue is empty) — idle-thread bookkeeping, not contention or allocator pressure. `brk`/`mmap` calls are a rounding error, all at startup.

Two concrete next steps if latency needs to come down further:

1. Pool-allocate the `PriceLevel` order lists. `std::list<OrderId>` heap-allocates a node per resting order; a slab-allocated structure would remove per-order `new`/`delete` from the hot path.
2. Replace the book's `shared_mutex` with a seqlock. There's one writer (the matcher) and many readers (TUI, snapshots) — a seqlock lets readers proceed with no atomic read-modify-write at all: the writer bumps a sequence counter around each mutation, and readers just retry if they catch an odd count mid-read.
```

(Same note: write the triple-backtick fences literally, not escaped, in the actual file.)

- [ ] **Step 3: Verify links and build still works**

```bash
make build
make test
```
Expected: unaffected (docs-only change), both still pass. Manually confirm `docs/DESIGN.md` renders sensibly and the README's link to it resolves.

- [ ] **Step 4: Commit**

```bash
git add README.md docs/DESIGN.md
git commit -m "docs: split README into quickstart + docs/DESIGN.md deep dive

Moves Technical Design and Profiling Analysis out of the README and
into a separate deep-dive doc, reordered the README so Build & Run and
Commands appear before Architecture, and rewrote the moved content to
remove AI-generated writing tells (mid-sentence emphasis bold, 'this
design means...' transitions, repetitive parallel structure per
section)."
```
