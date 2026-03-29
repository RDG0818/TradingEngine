# Frontend Design Spec — Talat Trading Dashboard

**Date:** 2026-03-29
**Status:** Approved
**Stack:** React 19 + TypeScript + Vite + Tailwind CSS (dark mode)

---

## Overview

A single-page, dark-mode trading dashboard for the Talat simulated exchange. The UI is observe-first — the order book and price chart are always visible. Automated traders can be monitored and toggled. Market events can be triggered. The user can optionally trade manually through a portfolio tab.

Single symbol: BTC/USD (synthetic, seeded from CoinGecko at startup).

---

## Layout

Two-column layout, no scrolling on the outer shell:

- **Left panel** (~32% width, full height): Order book — always visible, always live.
- **Right panel** (remaining width, full height): Price chart on top, tabbed content below.

```
┌─────────────────┬──────────────────────────────────┐
│                 │         Candlestick Chart         │
│   Order Book    ├──────────────────────────────────┤
│  (full height)  │  Trades │ Traders │ Events │ Me  │
│                 │         Tab Content               │
└─────────────────┴──────────────────────────────────┘
```

The chart takes roughly the top 35% of the right panel. The tab content fills the rest.

---

## Component Tree

```
App
├── OrderBook                   (left panel)
└── RightPanel
    ├── CandlestickChart
    ├── TabBar                  (Trades | Traders | Events | Portfolio)
    └── TabContent
        ├── TradesPanel
        ├── TradersPanel
        ├── EventsPanel
        └── PortfolioPanel
```

---

## Data Flow

### WebSocket (`/ws`)

Single persistent connection managed by a `useWebSocket` hook with exponential backoff reconnect. Dispatches two message types:

- `book_update` → replaces OrderBook state (full snapshot each time)
- `trade` → appended to TradesPanel list + fed to `useCandlesticks` hook

### HTTP — on mount (initial population)

| Endpoint | Used by |
|---|---|
| `GET /book_snapshot` | OrderBook (seed before first WS update) |
| `GET /recent_trades` | TradesPanel + CandlestickChart history |
| `GET /traders` | TradersPanel |
| `GET /metrics` | Header price/stats bar |
| `GET /portfolio/{user_trader_id}` | PortfolioPanel |

### HTTP — on demand

| Endpoint | Trigger |
|---|---|
| `POST /events/trigger` | Event button click |
| `POST /traders/{id}/toggle` | Trader row toggle |
| `GET /traders` | Re-fetched after any toggle |
| `POST /orders/limit` | Manual limit order submit |
| `POST /orders/market` | Manual market order submit |

### Polling

- `GET /traders` — every 2s (PnL updates)
- `GET /portfolio/{user_trader_id}` — every 2s (balance, position, PnL)

---

## Components

### OrderBook

Classic exchange-style table. Three columns: PRICE / QTY / TOTAL (cumulative).

- Asks above the spread line, bids below.
- Depth bar behind each row: red bars grow from the right on the ask side, green bars grow from the left on the bid side. Bar width is proportional to cumulative quantity relative to the max cumulative value shown.
- Spread line in the center showing last trade price and spread in dollars.
- Shows top 15 levels per side.
- Re-renders on every `book_update` WS message.

### CandlestickChart

Built with Recharts `ComposedChart`. Candles are 10-second OHLC buckets aggregated client-side from the WS `trade` stream.

- Seeded with `GET /recent_trades` on mount.
- Each new trade either extends the current open candle or starts a new bucket.
- Displays the last ~60 candles. Older candles are dropped from state.
- Auto-scrolls to the latest candle.
- Green candles for close ≥ open, red for close < open.
- Y-axis auto-scales to visible candle range with 5% padding.

Custom hook: `useCandlesticks(trades: Trade[], intervalMs: number): Candle[]`

### TradesPanel

Scrolling feed of fills, newest at top.

- Seeded from `GET /recent_trades`. New trades prepended via WS `trade` events.
- Capped at 200 entries in state.
- Each row: price (green for buy, red for sell), quantity, timestamp.

### TradersPanel

Dense table showing all automated traders. Columns: NAME / TYPE / PNL / FILLS / toggle.

- TYPE shown as abbreviation (MM, MOM, MR, RL, RM, TWAP, TF, PANIC).
- PnL colored green (positive) or red (negative).
- Toggle is a visual switch; clicking immediately calls `POST /traders/{id}/toggle` and re-fetches the trader list.
- Inactive traders shown at reduced opacity.
- Data refreshed every 2s via polling.

### EventsPanel

Four event trigger buttons arranged in a 2×2 grid:

- Flash Crash
- Bull Run
- Liquidity Squeeze
- Mean Reversion Trap

Each button calls `POST /events/trigger` with the appropriate event type and a default `duration_ticks` of 30. Active events (if trackable from the API) are visually highlighted. Buttons are disabled briefly after trigger to prevent double-fire.

### PortfolioPanel

Two sections stacked vertically:

**Stats (top half):**
- Balance (USD)
- BTC position (signed)
- Unrealized PnL
- Avg cost

Updated every 2s via `GET /portfolio/{user_trader_id}`.

**Order Entry (bottom half):**
- Side toggle: BUY / SELL
- Type toggle: LIMIT / MARKET
- Price input (hidden when type is MARKET)
- Quantity input
- Submit button — calls `POST /orders/limit` or `POST /orders/market`
- Feedback: brief success/error flash below the form on submit

The user's portfolio is created at app startup via `POST /portfolio` and the returned `trader_id` is stored in component state for the session.

---

## State Management

No external state library. React state + two custom hooks:

- **`useWebSocket(url)`** — opens connection, handles reconnect with exponential backoff, exposes `lastMessage` and `status`. Callers register handlers by message type.
- **`useCandlesticks(trades, intervalMs)`** — pure aggregation hook; takes a flat array of trades and returns OHLC candle array.

Global data (WS connection, user trader ID) passed via React context. Component-local data (order form state, tab selection) stays local.

---

## Theming

Tailwind dark mode (`class` strategy). Color palette:

- Background: `#0a0a0a` (page), `#0f0f0f` (panels), `#111111` (cards/rows)
- Border: `#1a1a1a` standard, `#2a2a2a` dividers
- Green (bids / buy / positive): `#4ade80`
- Red (asks / sell / negative): `#ef4444`
- Muted text: `#888888`
- Labels: `#555555`
- Monospace font throughout (order book, chart axes, trade feed)

---

## Out of Scope

- Authentication
- Multiple symbols
- Order cancellation UI (backend supports it; not exposed in this UI)
- Stop orders (backend supports them; manual entry is limit + market only)
- Order history / audit trail tab
- Responsive/mobile layout
