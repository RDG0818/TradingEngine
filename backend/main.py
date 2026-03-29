import asyncio
import json
import logging
from contextlib import asynccontextmanager
from typing import Optional

import httpx
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

import trading_engine_py as te

logger = logging.getLogger("talat")

# ---------------------------------------------------------------------------
# Global state
# ---------------------------------------------------------------------------

exchange: Optional[te.Exchange] = None
user_portfolio_id: Optional[int] = None  # default human trader

# WebSocket connections + async queue bridging C++ callbacks → WS push
ws_clients: set[WebSocket] = set()
ws_queue: asyncio.Queue = asyncio.Queue()
event_loop: Optional[asyncio.AbstractEventLoop] = None

# ---------------------------------------------------------------------------
# CoinGecko seed price
# ---------------------------------------------------------------------------

COINGECKO_URL = (
    "https://api.coingecko.com/api/v3/simple/price"
    "?ids=bitcoin&vs_currencies=usd"
)
FALLBACK_BTC_PRICE = 65000.0


async def fetch_btc_price() -> float:
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.get(COINGECKO_URL)
            r.raise_for_status()
            return float(r.json()["bitcoin"]["usd"])
    except Exception as e:
        logger.warning(f"CoinGecko fetch failed ({e}), using fallback ${FALLBACK_BTC_PRICE:,.0f}")
        return FALLBACK_BTC_PRICE


def to_price(dollars: float) -> int:
    """Convert dollar float to fixed-point price units (10000 = $1)."""
    return int(dollars * 10000)


def from_price(units: int) -> float:
    """Convert fixed-point price units to dollars."""
    return units / 10000


# ---------------------------------------------------------------------------
# WebSocket broadcast background task
# ---------------------------------------------------------------------------

async def ws_broadcaster():
    """Drain ws_queue and push each message to all connected clients."""
    while True:
        msg = await ws_queue.get()
        dead = set()
        for ws in ws_clients:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.add(ws)
        ws_clients.difference_update(dead)


# ---------------------------------------------------------------------------
# App lifecycle
# ---------------------------------------------------------------------------

@asynccontextmanager
async def lifespan(app: FastAPI):
    global exchange, user_portfolio_id, event_loop

    event_loop = asyncio.get_running_loop()

    # Fetch seed price
    btc_usd = await fetch_btc_price()
    seed_price = to_price(btc_usd)
    logger.info(f"BTC seed price: ${btc_usd:,.2f} ({seed_price} units)")

    # Create and start exchange
    exchange = te.Exchange()

    # Register C++ → asyncio bridge callbacks
    def on_fill(fill: te.Fill):
        msg = json.dumps({
            "type": "trade",
            "data": {
                "price":          from_price(fill.fill_price),
                "qty":            fill.fill_qty,
                "maker_order_id": fill.maker_order_id,
                "taker_order_id": fill.taker_order_id,
                "maker_trader_id":fill.maker_trader_id,
                "taker_trader_id":fill.taker_trader_id,
            },
        })
        event_loop.call_soon_threadsafe(ws_queue.put_nowait, msg)

    def on_book_update(snap: te.BookSnapshot):
        msg = json.dumps({
            "type": "book_update",
            "data": {
                "bids": [[from_price(p), q] for p, q in snap.bids[:20]],
                "asks": [[from_price(p), q] for p, q in snap.asks[:20]],
            },
        })
        event_loop.call_soon_threadsafe(ws_queue.put_nowait, msg)

    exchange.on_fill_callback(on_fill)
    exchange.on_book_update_callback(on_book_update)
    exchange.start(seed_price)

    # Seed default automated traders
    mm  = exchange.add_market_maker("market_maker", 10_000_000_000, seed_price)
    rlt = exchange.add_random_limit_trader("rand_limit", 2_000_000_000)
    rmt = exchange.add_random_market_trader("rand_market", 2_000_000_000)
    mom = exchange.add_momentum_trader("momentum", 5_000_000_000)
    rev = exchange.add_mean_reversion_trader("mean_reversion", 5_000_000_000)
    tf  = exchange.add_trend_follower("trend_follower", 5_000_000_000)
    for tid in [mm, rlt, rmt, mom, rev, tf]:
        exchange.start_trader(tid)

    # Default human portfolio
    user_portfolio_id = exchange.create_portfolio(10_000_000_000)  # $1M

    # Start WS broadcaster
    broadcaster_task = asyncio.create_task(ws_broadcaster())

    logger.info("Exchange started.")
    yield

    broadcaster_task.cancel()
    if exchange and exchange.is_running():
        exchange.stop()
    logger.info("Exchange stopped.")


app = FastAPI(title="Talat Trading Engine", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://localhost:3000", "http://localhost"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ---------------------------------------------------------------------------
# Pydantic models
# ---------------------------------------------------------------------------

class TraderCreate(BaseModel):
    name: str
    type: str  # "market_maker" | "momentum" | "mean_reversion" | "twap" | "trend_follower" | "random_limit" | "random_market"
    balance: int = 5_000_000_000
    # TWAP-specific
    total_qty: int = 100
    slices: int = 10
    is_buy: bool = True

class OrderCreate(BaseModel):
    trader_id: int
    is_buy: bool
    price: float         # dollars
    qty: int
    tif: str = "GTC"     # GTC | IOC | FOK

class MarketOrderCreate(BaseModel):
    trader_id: int
    is_buy: bool
    qty: int

class EventTrigger(BaseModel):
    type: str            # flash_crash | bull_run | liquidity_squeeze | mean_reversion_trap
    duration_ticks: int = 30

class PortfolioReset(BaseModel):
    trader_id: int
    balance: int = 10_000_000_000


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

EVENT_TYPE_MAP = {
    "flash_crash":         te.MarketEventType.FlashCrash,
    "bull_run":            te.MarketEventType.BullRun,
    "liquidity_squeeze":   te.MarketEventType.LiquiditySqueeze,
    "mean_reversion_trap": te.MarketEventType.MeanReversionTrap,
}

def _require_exchange() -> te.Exchange:
    if exchange is None or not exchange.is_running():
        raise HTTPException(status_code=503, detail="Exchange is not running")
    return exchange


def _trader_info_dict(info: te.TraderInfo) -> dict:
    return {
        "id":      info.id,
        "name":    info.name,
        "type":    info.type,
        "active":  info.active,
        "metrics": {
            "orders_per_second": info.metrics.orders_per_second,
            "pnl":               info.metrics.pnl,
            "position":          info.metrics.position,
        },
    }


# ---------------------------------------------------------------------------
# Engine endpoints
# ---------------------------------------------------------------------------

@app.get("/")
def root():
    return {"status": "ok", "engine": exchange.is_running() if exchange else False}


@app.post("/engine/start")
def engine_start():
    if exchange and not exchange.is_running():
        exchange.start(to_price(FALLBACK_BTC_PRICE))
    return {"running": exchange.is_running() if exchange else False}


@app.post("/engine/stop")
def engine_stop():
    if exchange and exchange.is_running():
        exchange.stop()
    return {"running": False}


@app.get("/engine/status")
def engine_status():
    return {"running": exchange.is_running() if exchange else False}


# ---------------------------------------------------------------------------
# Market data
# ---------------------------------------------------------------------------

@app.get("/book_snapshot")
def book_snapshot():
    ex = _require_exchange()
    snap = ex.book_snapshot()
    return {
        "bids": [[from_price(p), q] for p, q in snap.bids],
        "asks": [[from_price(p), q] for p, q in snap.asks],
    }


@app.get("/recent_trades")
def recent_trades(limit: int = 50):
    ex = _require_exchange()
    fills = ex.recent_trades(limit)
    return [
        {
            "price":           from_price(f.fill_price),
            "qty":             f.fill_qty,
            "maker_order_id":  f.maker_order_id,
            "taker_order_id":  f.taker_order_id,
            "maker_trader_id": f.maker_trader_id,
            "taker_trader_id": f.taker_trader_id,
        }
        for f in fills
    ]


@app.get("/metrics")
def metrics():
    ex = _require_exchange()
    m = ex.metrics()
    return {
        "orders_processed":  m.orders_processed,
        "avg_latency_us":    m.avg_latency_us,
        "throughput_per_s":  m.throughput_per_s,
        "last_trade_price":  from_price(m.last_trade_price),
    }


# ---------------------------------------------------------------------------
# Portfolio
# ---------------------------------------------------------------------------

@app.get("/portfolio/default_id")
def portfolio_default_id():
    return {"trader_id": user_portfolio_id}


@app.get("/portfolio/{trader_id}")
def portfolio_snapshot(trader_id: int):
    ex = _require_exchange()
    snap = ex.portfolio_snapshot(trader_id)
    return {
        "balance":        snap.balance,
        "balance_usd":    from_price(snap.balance),
        "position":       snap.position,
        "unrealized_pnl": snap.unrealized_pnl,
        "avg_cost":       from_price(snap.avg_cost),
    }


@app.post("/portfolio/reset")
def portfolio_reset(req: PortfolioReset):
    ex = _require_exchange()
    # Re-create portfolio with fresh balance (Exchange doesn't expose reset directly)
    # Workaround: create a new portfolio ID and return it
    new_id = ex.create_portfolio(req.balance)
    global user_portfolio_id
    if req.trader_id == user_portfolio_id:
        user_portfolio_id = new_id
    return {"trader_id": new_id, "balance": req.balance}


# ---------------------------------------------------------------------------
# Orders
# ---------------------------------------------------------------------------

@app.post("/orders/limit")
def submit_limit_order(order: OrderCreate):
    ex = _require_exchange()
    order_id = ex.submit_limit_order(
        order.trader_id,
        order.is_buy,
        to_price(order.price),
        order.qty,
        order.tif,
    )
    return {"order_id": order_id}


@app.post("/orders/market")
def submit_market_order(order: MarketOrderCreate):
    ex = _require_exchange()
    order_id = ex.submit_market_order(order.trader_id, order.is_buy, order.qty)
    return {"order_id": order_id}


@app.delete("/orders/{order_id}")
def cancel_order(order_id: int):
    ex = _require_exchange()
    ex.cancel_order(order_id)
    return {"cancelled": order_id}


# ---------------------------------------------------------------------------
# Automated traders
# ---------------------------------------------------------------------------

@app.get("/traders")
def list_traders():
    ex = _require_exchange()
    return {"traders": [_trader_info_dict(t) for t in ex.all_traders()]}


@app.post("/traders", status_code=201)
def create_trader(body: TraderCreate):
    ex = _require_exchange()
    t = body.type
    try:
        if t == "market_maker":
            m = ex.metrics()
            seed = m.last_trade_price or to_price(FALLBACK_BTC_PRICE)
            tid = ex.add_market_maker(body.name, body.balance, seed)
        elif t == "momentum":
            tid = ex.add_momentum_trader(body.name, body.balance)
        elif t == "mean_reversion":
            tid = ex.add_mean_reversion_trader(body.name, body.balance)
        elif t == "twap":
            tid = ex.add_twap_trader(body.name, body.balance, body.total_qty, body.slices, body.is_buy)
        elif t == "trend_follower":
            tid = ex.add_trend_follower(body.name, body.balance)
        elif t == "random_limit":
            tid = ex.add_random_limit_trader(body.name, body.balance)
        elif t == "random_market":
            tid = ex.add_random_market_trader(body.name, body.balance)
        else:
            raise HTTPException(status_code=400, detail=f"Unknown trader type: {t!r}")
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    return {"trader_id": tid}


@app.delete("/traders/{trader_id}")
def delete_trader(trader_id: int):
    ex = _require_exchange()
    ex.remove_trader(trader_id)
    return {"removed": trader_id}


@app.post("/traders/{trader_id}/start")
def start_trader(trader_id: int):
    ex = _require_exchange()
    ex.start_trader(trader_id)
    return {"trader_id": trader_id, "active": True}


@app.post("/traders/{trader_id}/stop")
def stop_trader(trader_id: int):
    ex = _require_exchange()
    ex.stop_trader(trader_id)
    return {"trader_id": trader_id, "active": False}


@app.post("/traders/{trader_id}/toggle")
def toggle_trader(trader_id: int):
    ex = _require_exchange()
    info = next((t for t in ex.all_traders() if t.id == trader_id), None)
    if info is None:
        raise HTTPException(status_code=404, detail=f"Trader {trader_id} not found")
    if info.active:
        ex.stop_trader(trader_id)
        return {"trader_id": trader_id, "active": False}
    else:
        ex.start_trader(trader_id)
        return {"trader_id": trader_id, "active": True}


# ---------------------------------------------------------------------------
# Market events
# ---------------------------------------------------------------------------

@app.get("/events")
def list_events():
    return {"events": [
        {"id": e.id, "name": e.name, "description": e.description,
         "default_duration_s": e.default_duration_s}
        for e in te.all_market_events()
    ]}


@app.post("/events/trigger")
def trigger_event(body: EventTrigger):
    ex = _require_exchange()
    event_type = EVENT_TYPE_MAP.get(body.type)
    if event_type is None:
        raise HTTPException(status_code=400, detail=f"Unknown event type: {body.type!r}")
    ex.trigger_event(event_type, body.duration_ticks)
    return {"triggered": body.type, "duration_ticks": body.duration_ticks}


# ---------------------------------------------------------------------------
# WebSocket
# ---------------------------------------------------------------------------

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    try:
        while True:
            await ws.receive_text()  # keep alive; client can send pings
    except WebSocketDisconnect:
        pass
    finally:
        ws_clients.discard(ws)
