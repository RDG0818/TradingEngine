# Talat

A high-performance simulated trading exchange in pure C++20 with a fullscreen terminal UI. *Talat* (ตลาด) means *market* in Thai.

```
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
```

## Performance

Benchmarked on an Intel i7-1255U (12-thread), Release build:

| Metric | Result |
|---|---|
| Submit throughput | **17M orders/sec** (lock-free queue) |
| Match latency p50 | **108 µs** |
| Match latency p99 | **144 µs** |
| Match latency p99.9 | **215 µs** |
| Write throughput (4 threads) | **6.7M orders/sec** |
| Book snapshot reads (8 threads) | **940K snapshots/sec** |

## Architecture

```
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
```

**Order types:** Limit (GTC/IOC/FOK), Market (IOC), Stop-Limit, Stop-Market

**Automated traders** run on a configurable tick thread and drive realistic price action:

| Trader | Behavior |
|---|---|
| `MarketMaker` | Quotes bid+ask ± GBM fair value. Widens spread up to 3× under adverse selection (Glosten-Milgrom). |
| `InformedTrader` | Noisy signal = latent × (1 + N(0,σ)). Submits IOC when signal diverges > 0.2% from last price. |
| `NoiseTrader` | Poisson arrivals (λ=0.7). 60% limit / 40% market split. Log-normal sizes. Random side. |

**Prices** are `uint64_t` fixed-point: `10000 = $1.00` (e.g. $64,200 → `642000000`).

## Technical Design

### Matching Engine

The `OrderMatcher` runs on a single dedicated worker thread and processes all order operations through a **moodycamel `ConcurrentQueue<Command>`** — a lock-free MPSC queue. Submits and cancels are wrapped in a `std::variant<SubmitCmd, CancelCmd>` and enqueued from any thread; the worker drains them in a tight `bulk_dequeue` loop. This design means all book mutations happen on one thread with no locks, which is why raw submit throughput hits 17M/s.

Order processing flow:
1. Enqueue timestamp (`submit_tp`) is captured at dequeue time and passed through to `OrderAcceptedEvent` for latency measurement.
2. Limit orders call `try_match_limit`, which walks the opposite side of the book filling against resting levels until the order is exhausted, a price-level mismatch stops it, or TIF rules (IOC/FOK) force a cancel.
3. Market orders walk the book with no price constraint.
4. Stop orders sit in `pending_stop_limits_` / `pending_stop_markets_` maps and are triggered by `check_stop_orders` after every fill.
5. Self-match prevention: if maker and taker share the same `TraderId`, the fill is skipped.
6. Partial fills are tracked per order in `filled_qty_` (`unordered_map<OrderId, Quantity>`).

### Order Book

The book uses two **`boost::container::flat_map<Price, PriceLevel>`** — one for bids, one for asks. `flat_map` stores its key-value pairs in a contiguous sorted array rather than a tree, giving O(log n) lookups with much better cache behavior than `std::map`. Both sides are stored ascending; bids are walked `rbegin→rend` (highest first), asks `begin→end` (lowest first).

Each `PriceLevel` holds a `total_qty` and a `std::list<OrderId>` for FIFO ordering within the level. A separate `unordered_map<OrderId, LimitOrder>` stores the full order structs for O(1) lookup by ID.

Concurrent access uses a **`shared_mutex`**: readers (TUI snapshot, informed traders checking best bid/ask) take shared locks; the matcher worker takes exclusive locks only during mutations. The `snapshot()` method copies bid/ask levels under the lock and returns a plain `vector<pair<Price, Quantity>>` so the TUI never holds the book lock while rendering.

### EventBus

A header-only type-erased pub/sub system. Subscriptions are registered as `std::function<void(const EventType&)>` and stored in an `unordered_map<std::type_index, vector<Handler>>`. On publish, the event is wrapped in `std::any` and dispatched to all handlers for that type via a **shared lock** — multiple publishers can fire simultaneously without blocking each other; only subscription/unsubscription takes an exclusive lock. Each subscription returns a `SubscriptionToken` (monotonic `uint64_t`) for cleanup.

Events in the system:
- `FillEvent` — emitted after every match; carries the full `Fill` struct including `taker_side` for correct portfolio attribution
- `BookUpdateEvent` — emitted after any book mutation; triggers TUI re-render
- `OrderAcceptedEvent` — carries `submit_tp` for latency tracking by `StatsTracker`
- `OrderRejectedEvent`, `OrderCancelledEvent`

### Latent Price (GBM)

`LatentPrice` models a theoretical fair value using **zero-drift Geometric Brownian Motion**. Each tick advances the log-price by `σ · Z` where Z ~ N(0,1):

```
log_price += σ * Z
price = exp(log_price) * 10000   // back to fixed-point
```

The current price is stored in a **`std::atomic<Price>`** so any trader thread can call `get()` with acquire semantics and no lock. `sigma` defaults to 0.0003 (~0.03% per tick), controllable at runtime via `/vol`.

### Automated Traders

All three types extend `Trader` and are driven by `TraderRegistry`, which runs a configurable tick loop (default 200ms) on its own thread. Each tick calls `latent_.tick()` to advance the GBM, then dispatches to each active trader via `submit` and `cancel` lambdas that enqueue to the matcher.

**MarketMaker** — quotes a bid and ask symmetrically around the GBM price, cancelling and requoting every tick. Tracks fill rate over a 20-tick sliding window; when fill rate exceeds 30% (a signal that informed traders are picking it off), it widens the half-spread by up to 3× using the formula `multiplier = min(1 + fill_rate * 2, 3.0)`. This is the Glosten-Milgrom insight: a rational market maker widens quotes when adverse selection risk is high. The effective spread persists across window resets so there's no instantaneous snap-back.

**InformedTrader** — models a trader with a noisy signal about true value. Each tick it draws a signal: `signal = latent * (1 + N(0, signal_σ))`. If the signal deviates from the last trade price by more than a threshold (default 0.2%), it submits an IOC limit at the signal price. IOC means it either fills immediately or cancels — no resting orders, just directional pressure that drives price discovery.

**NoiseTrader** — pure uninformed flow. Order arrivals are Poisson-distributed (`λ = 0.7` per tick, so ~70% chance of an order each tick). Side is 50/50. Size is log-normally distributed (mean ~2 units). Order type is 60% limit (placed near mid) / 40% market. Provides the background liquidity and noise that makes the simulation realistic.

### Portfolio & PnL

Each portfolio tracks `balance`, `position`, `total_cost`, and `total_bought` (the latter two for avg cost). On every fill, `apply_fill` is called with the correct side derived from `Fill::taker_side` — takers get `taker_side`, makers get the opposite.

PnL is computed as:
- **Unrealized** = `position × (current_price − avg_cost)`
- **Realized** = `balance − starting_balance + position × avg_cost` (cash flow since start, net of open position at cost — removes the unrealized component)
- **Total** = `balance − starting_balance + position × current_price` (mark-to-market)

### StatsTracker

Subscribes to `OrderAcceptedEvent` and maintains a **rolling 5-second deque** of `(timestamp, latency_µs)` records. `snapshot()` filters to the window, sorts latencies, and returns p50/p99 and orders/sec. Latency is measured from `submit_tp` (captured when the command is dequeued by the matcher worker) to when the `OrderAcceptedEvent` is published — so it reflects queue drain time plus book insertion, not roundtrip to the caller.

### Fixed-Point Arithmetic

All prices are `uint64_t` with an implicit scale of **10,000 units = $1.00**. This avoids floating-point rounding in financial calculations. Example: $64,200.50 → `642,005,000`. The TUI and order parser convert to/from `double` only at the user boundary.

### Profiling Analysis

Profiled with `perf stat` and `perf record` on the throughput benchmark (Release build, i7-1255U).

**`perf stat` summary:**

| Counter | Value | Meaning |
|---|---|---|
| Context switches | **0** | Lock-free queue never blocks the submitting thread |
| Branch miss rate | **1.5%** | Matcher control flow is highly predictable |
| Backend bound | **50%** | Memory-latency-limited, not compute-limited |
| IPC | **0.44** | Low instructions-per-cycle, consistent with pointer-chasing bottlenecks |

**Hot path breakdown** (`perf report`, inside `OrderMatcher::run_loop → try_match_limit`):

- **~10% of `add_order` in `operator new`** — `PriceLevel` holds a `std::list<OrderId>`, which heap-allocates a separate node per resting order. This is the single largest unnecessary allocation in the hot path.
- **~16% in lock primitives** — `rwlock_wrlock` (3.9%) + `rwlock_unlock` (7.7%) + `rwlock_rdlock` (4.5%). The `shared_mutex` guarding the book is taken on every mutation and snapshot.
- **5% in `std::_Hash_bytes`** — hashing `OrderId` for the `unordered_map<OrderId, LimitOrder>` that backs O(1) order lookup.
- **7% in `OrderBook::snapshot()`** — copying bid/ask vectors under the read lock, triggered by `BookUpdateEvent` on every fill or add.

**Two concrete optimization targets** if pushing latency further:

1. **Pool-allocate `PriceLevel` order lists** — replace `std::list<OrderId>` with a `std::vector` backed by a per-level slab allocator. Price levels are long-lived and frequently reused; eliminating per-order `new`/`delete` would cut the `add_order` cost roughly in half.

2. **Replace `shared_mutex` with a seqlock** — the book has one writer (the matcher thread) and many readers (TUI, snapshots). A seqlock lets readers proceed with no atomic RMW: the writer increments a sequence counter before and after each mutation; readers spin-retry if they observe an odd count. This would reclaim most of the 16% spent in lock primitives.

## Requirements

- C++20 compiler (GCC 12+ or Clang 14+)
- CMake 3.14+
- Boost 1.74+

## Build & Run

```bash
git clone --recurse-submodules https://github.com/RDG0818/TradingEngine.git
cd TradingEngine
make build    # cmake + compile (fetches ftxui on first run)
make run      # ./build/trading_engine
make run -- --seed 50000   # seed at $50,000
```

```bash
make test     # 48 tests, 12 suites
make bench    # throughput, latency, contention benchmarks
```

## Commands

Type commands in the bottom bar:

```
buy <qty> @ <price>          limit buy (GTC)
sell <qty> @ <price>         limit sell (GTC)
buy <qty>                    market buy (IOC)
sell <qty>                   market sell (IOC)
cancel <order_id>            cancel a resting order
q / quit                     exit
```

Slash commands to control the simulation:

```
/vol <0.0–1.0>               GBM volatility σ       (default: 0.0003)
/spread <dollars>            market maker half-spread (default: $2.00)
/speed <1–10>                tick speed, 1=2s 10=200ms (default: 1)
/pause / /resume             halt or restart all automated traders
/traders <mm|informed|noise> <n>   set trader count per type
/help                        show command reference
```

## License

MIT — see `LICENSE` for details.
