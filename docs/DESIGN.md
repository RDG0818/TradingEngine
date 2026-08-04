# Talat — Design Notes

Deep dive on the matching engine, concurrency model, and profiling results. See the [README](../README.md) for build/run instructions and the command reference.

## Matching Engine

OrderMatcher runs on one dedicated worker thread. All order and cancel commands land in a moodycamel ConcurrentQueue — a lock-free MPSC queue — wrapped in a `std::variant<SubmitCmd, CancelCmd>`, so any thread can enqueue without blocking. The worker drains the queue in a loop of `queue_.try_dequeue(cmd)` calls, falling back to a short `sleep_for` when the queue comes up empty (see `run_loop` in `order_matcher.cpp`). The lock-free queue means callers submitting orders never block on each other or on the matcher — that's what drives the 17M/s enqueue throughput figure, which measures pushing into the queue, not book mutation. Book mutation itself still goes through `OrderBook`'s `shared_mutex` (`unique_lock` for `add_order`/`cancel_order`, `shared_lock` for reads), as described in the Order Book section below — the matching path is not lock-free.

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

```
log_price += σ * Z
price = exp(log_price) * 10000
```

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

`perf stat`'s key counters: IPC sits around 0.55–0.61 on both core types, meaning the pipeline is roughly a quarter utilized — this workload is memory-latency-bound, not compute-bound. Cache miss rate runs 25–29%, which tracks with the `flat_map`/`unordered_map` working set exceeding L1 under load. Futex time totals 0.638ms — lock contention is negligible. Wall-clock sys time comes out to 34%, which looked concerning until a syscall trace (`strace -c`) showed it's almost entirely `clock_nanosleep` — both the trader registry's tick thread sleeping between ticks and the matcher's `run_loop` calling `std::this_thread::sleep_for(100µs)` whenever `try_dequeue` comes up empty resolve to the same syscall (there's no `std::this_thread::yield()`/`sched_yield` anywhere in the codebase) — idle-thread bookkeeping, not contention or allocator pressure. `brk`/`mmap` calls are a rounding error, all at startup.

Two concrete next steps if latency needs to come down further:

1. Pool-allocate the `PriceLevel` order lists. `std::list<OrderId>` heap-allocates a node per resting order; a slab-allocated structure would remove per-order `new`/`delete` from the hot path.
2. Replace the book's `shared_mutex` with a seqlock. There's one writer (the matcher) and many readers (TUI, snapshots) — a seqlock lets readers proceed with no atomic read-modify-write at all: the writer bumps a sequence counter around each mutation, and readers just retry if they catch an odd count mid-read.
