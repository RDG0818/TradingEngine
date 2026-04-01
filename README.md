# Talat

A high-performance simulated trading exchange in pure C++20 with a fullscreen terminal UI. *Talat* (ตลาด) means *market* in Thai.

Built as a systems/quant portfolio piece showcasing lock-free concurrency, market microstructure, and real-time terminal rendering — no Python, no web stack, just a self-contained binary.

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
