# Trading Engine Refactor — Design Spec
*2026-03-28*

## Context

The existing codebase (Talat) accumulated complexity across the C++ core, Python bridge, and React frontend as each was built in isolation without a clear end-to-end vision. The C++ core has good fundamentals but suffers from excessive mutexes, mutable order state, and tight coupling. The Python API is unreadable. The frontend is poorly structured with broken integrations (LiveEventLog uses hardcoded placeholder data).

The goal of this refactor is threefold:
1. Produce a clean, modular, well-optimized C++ order book that is readable and teachable.
2. Build a clean observe-first frontend where users can watch a live simulated market and understand how order matching works at a fundamental level.
3. Establish a strong foundation for future directions (additional strategies, educational modes, etc.).

---

## C++ Core

### Renamed Types

| Old | New |
|-----|-----|
| `TradingSystem` | `Exchange` |
| `MatchingEngine` | `OrderMatcher` |
| `EventDispatcher` | `EventBus` |
| `TraderManager` | `TraderRegistry` |
| `TradeExecutedEvent` | `Trade` |
| `BookUpdateEvent` | `BookUpdate` |
| `OrderFilledEvent` | `Fill` |

`OrderBook` and `Portfolio` stay as-is.

### Order Representation — `std::variant`

Replace the polymorphic `Order` base class with plain structs and a variant:

```cpp
struct LimitOrder  { OrderId id; TraderId trader; Side side; Price price; Quantity qty; TimeInForce tif; Timestamp ts; };
struct MarketOrder { OrderId id; TraderId trader; Side side; Quantity qty; TimeInForce tif; Timestamp ts; };
struct StopLimitOrder  { OrderId id; TraderId trader; Side side; Price stop_price; Price limit_price; Quantity qty; Timestamp ts; };
struct StopMarketOrder { OrderId id; TraderId trader; Side side; Price stop_price; Quantity qty; Timestamp ts; };

using Order = std::variant<LimitOrder, MarketOrder, StopLimitOrder, StopMarketOrder>;
```

Orders are **immutable** once created. No quantity mutation. `std::visit` replaces all type-switching. No heap allocation per order type, no virtual dispatch.

### Fill Records — Audit Trail

Each (partial) execution produces a `Fill` record instead of mutating order quantity:

```cpp
struct Fill {
    OrderId  maker_order_id;
    OrderId  taker_order_id;
    TraderId maker_trader_id;
    TraderId taker_trader_id;
    Price    fill_price;
    Quantity fill_qty;
    Timestamp ts;
};
```

Order status (`open`, `partially_filled`, `filled`, `cancelled`) is derived by the `OrderMatcher` by summing fills against the original order quantity. This is the standard approach (FIX protocol style) and gives a complete audit trail.

### OrderBook — `boost::container::flat_map`

Replace `std::map<Price, PriceLevel>` with `boost::container::flat_map<Price, PriceLevel>`. Drop-in replacement with contiguous memory layout — significantly faster cache performance when walking the book during matching and snapshot generation. No readability cost.

Each `PriceLevel` holds a list of resting order IDs and the total quantity at that level.

Single `std::shared_mutex` per OrderBook (down from 3). Writers (add/remove order) take exclusive lock; readers (snapshot, for_each) take shared lock.

### Threading Model

- **OrderMatcher** runs on a dedicated worker thread, consuming from a moodycamel lock-free `ConcurrentQueue<MatcherEvent>`. Events: `SubmitOrder`, `CancelOrder`, `TriggerEvent`. No change from current — it's already correct.
- **TraderRegistry** runs on a separate tick thread. Implementation moved out of the header into `trader_registry.cpp`.
- **Exchange** (top-level) splits the current god-mutex into: `portfolio_mutex_` (guards portfolios) and `snapshots_mutex_` (guards cached snapshots for the Python layer).

### EventBus

Keep the existing `EventBus` design (type-safe pub/sub with `shared_mutex`). Add an `unsubscribe` token so traders can be cleanly removed without leaking callbacks.

### Price Representation

Unchanged: `uint64_t` fixed-point, 10000 = $1.00. Timestamps: `std::chrono::nanoseconds`.

### Market Seeding

On `Exchange::start()`, fetch the current BTC price from the CoinGecko public API (no auth). This becomes the `MarketMakerTrader`'s initial GBM reference price. If the fetch fails, fall back to a configurable default. The simulation runs fully independently from that point.

---

## Automated Traders

Eight trader types. All implement a common `Trader` interface with `tick()` and `reset()`.

| Type | Strategy | PnL Profile |
|------|----------|-------------|
| `MarketMakerTrader` | Quotes both sides via GBM, earns spread | Steady in choppy markets, bleeds in trends |
| `MomentumTrader` | Buys into strength, sells into weakness (recent price delta) | Thrives in trends, whipsaws in choppy markets |
| `MeanReversionTrader` | Fades moves, bets price returns to rolling mean | Thrives in choppy markets, bleeds in trends |
| `TWAPTrader` | Splits a large directional order into equal time slices | Shows execution quality vs. single large order |
| `TrendFollowerTrader` | Moving average crossover signal (slower than momentum) | Holds longer positions, lower turnover |
| `RandomLimitTrader` | Random limit orders, noise liquidity provider | Near-zero PnL, provides book depth |
| `RandomMarketTrader` | Random market orders, noise taker | Negative expected PnL (pays spread) |
| `PanicTrader` | Spawned by market events only (not user-created), dumps aggressively then expires | Causes sharp price dislocations |

### Market Events

Named presets that reconfigure the trader mix for a short duration:

| Event | Effect |
|-------|--------|
| `FlashCrash` | Spawns 3–5 PanicTraders, temporarily suspends MarketMakers |
| `BullRun` | Cranks MomentumTrader aggression, increases order size |
| `LiquiditySqueeze` | Suspends all RandomLimitTraders, widens spread |
| `MeanReversionTrap` | Activates large MomentumTrader buying into an oversold condition |

Events auto-expire after a configurable duration (default 30s) and the market returns to baseline.

---

## Python / FastAPI API

### REST Endpoints

**Engine**
```
POST /engine/start
POST /engine/stop
GET  /engine/status          → { running, uptime_s, seed_price }
```

**Market**
```
GET /orderbook               → { bids: [[price, qty]], asks: [[price, qty]], spread, mid_price }
GET /trades?limit=50         → [{ price, qty, side, ts }]
GET /metrics                 → { orders_processed, avg_latency_us, throughput_per_s }
```

**Portfolio**
```
GET    /portfolio             → { balance, positions: { qty, avg_cost }, unrealized_pnl }
POST   /orders                → { side, type, qty, price? }  →  { order_id }
DELETE /orders/{order_id}
```

**Traders**
```
GET    /traders               → [{ name, type, status, pnl, position, orders_per_s }]
POST   /traders               → { type, name, params }
DELETE /traders/{name}
POST   /traders/{name}/start
POST   /traders/{name}/stop
PUT    /traders/{name}/params → { ...params }
```

**Events**
```
GET  /events                  → [{ id, name, description, duration_s }]
POST /events/trigger          → { event: "flash_crash" | "bull_run" | "liquidity_squeeze" | "mean_reversion_trap" }
```

### WebSocket

```
WS /ws
```

Two message types pushed to all connected clients:

```jsonc
// On every order book change
{ "type": "book_update", "bids": [[price, qty], ...], "asks": [[price, qty], ...], "mid": 64200.50 }

// On every fill — side is the aggressor (taker) side
{ "type": "trade", "price": 64200.50, "qty": 0.15, "side": "buy", "ts": 1711584000000 }
```

The Python layer subscribes to `BookUpdate` and `Fill` events from the C++ `EventBus` at startup and fans them out to all connected WebSocket clients.

---

## Frontend

**Stack:** React + TypeScript + Vite. Full rewrite — no code carried over.

**Layout — Book Dominant (Layout B):**

```
┌─────────────────────────────────────────────────────┐
│  HEADER: price · % change · engine status · latency │
├───────────────────┬─────────────────────────────────┤
│                   │  PRICE CHART                    │
│   ORDER BOOK      ├─────────────────────────────────┤
│   bids/asks       │  TRADE TAPE (scrolling)         │
│   depth bars      │                                 │
├───────────────────┴─────────────────────────────────┤
│  PORTFOLIO: PnL · positions                         │
├─────────────────────────────────────────────────────┤
│  BOTS PANEL: traders · PnL · status · event buttons │
└─────────────────────────────────────────────────────┘
```

**Data flow:**
- WebSocket `/ws` → order book + trade tape (real-time, no polling)
- REST polling at ~2s → portfolio, bot PnL, metrics
- REST commands → order submission, bot management, event triggers

**Routes:**
- `/` — main dashboard (above layout)
- `/trade` — minimal order form (limit/market, buy/sell, qty, price)

**State management:** React context for the shared WebSocket connection. Local state per component. No external state library.

**Visual design:** Intentionally not specified — to be designed separately in Figma/Stitch.

---

## File Structure (proposed)

```
TradingEngine/
├── include/
│   ├── exchange.h
│   ├── order_matcher.h
│   ├── order_book.h
│   ├── order.h              # variant types + Fill
│   ├── event_bus.h
│   ├── trader_registry.h
│   ├── trader.h             # Trader interface + all 8 types
│   ├── portfolio.h
│   └── market_events.h
├── src/
│   ├── exchange.cpp
│   ├── order_matcher.cpp
│   ├── order_book.cpp
│   ├── event_bus.cpp
│   ├── trader_registry.cpp
│   ├── traders/
│   │   ├── market_maker.cpp
│   │   ├── momentum.cpp
│   │   ├── mean_reversion.cpp
│   │   ├── twap.cpp
│   │   ├── trend_follower.cpp
│   │   ├── random_limit.cpp
│   │   ├── random_market.cpp
│   │   └── panic.cpp
│   ├── market_events.cpp
│   └── python_bindings.cpp
├── backend/
│   ├── main.py
│   ├── ws_manager.py        # WebSocket fan-out
│   └── requirements.txt
├── frontend/
│   └── trading_exchange_frontend/
│       ├── src/
│       │   ├── App.tsx
│       │   ├── context/WebSocketContext.tsx
│       │   ├── components/
│       │   │   ├── Header.tsx
│       │   │   ├── OrderBook.tsx
│       │   │   ├── PriceChart.tsx
│       │   │   ├── TradeTape.tsx
│       │   │   ├── Portfolio.tsx
│       │   │   └── BotsPanel.tsx
│       │   └── pages/
│       │       ├── Dashboard.tsx
│       │       └── Trade.tsx
│       └── ...
└── tests/
    └── cpp/
```

---

## Verification

1. **C++ build:** `make build` compiles cleanly with no warnings. `./build/my_tests` passes all unit tests.
2. **Python bindings:** Python can `import trading_engine_py` and call `Exchange.start()` / `Exchange.stop()`.
3. **Backend:** `uvicorn main:app --reload` starts. `GET /engine/status` returns `{ running: false }`. `POST /engine/start` starts it, WebSocket at `/ws` begins pushing `book_update` messages.
4. **Market seeding:** On start, BTC price fetched from CoinGecko. `GET /engine/status` shows `seed_price`.
5. **Traders:** `POST /traders` with each of the 8 types succeeds. `GET /traders` shows all with PnL. `POST /events/trigger` with `flash_crash` causes visible price dislocation in trade tape.
6. **Frontend:** Order book updates in real time via WebSocket. Trade tape streams fills. Bot PnL updates every 2s. Event buttons trigger visible market reactions.
