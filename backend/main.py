from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from typing import Optional
import trading_engine_py
import random
import time

trading_system: Optional[trading_engine_py.TradingSystem] = None
default_trader_id: Optional[int] = None

SYMBOLS = ["ETH-USD-PERP", "BTC-USD-SPOT"] # hardcoded for now

@asynccontextmanager
async def lifespan(app: FastAPI):
    global trading_system, default_trader_id
    trading_system = trading_engine_py.TradingSystem(100, SYMBOLS)
    print("TradingSystem singleton instance created.")

    if trading_system:
        default_trader_id = trading_system.create_portfolio(1000000)
        print(f"Default portfolio created with trader_id: {default_trader_id}")

    trading_system.start()
    print("TradingSystem singleton instance started.")

    yield
    if trading_system and trading_system.is_running():
        trading_system.stop()
        print("TradingSystem singleton instance stopped.")

app = FastAPI(lifespan=lifespan)

# CORS configuration

origins = [
    "http://localhost",
    "http://localhost:3000",
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Engine Endpoints
@app.post("/engine/start")
def start_engine():
    if trading_system and not trading_system.is_running():
        trading_system.start()
        print("Trading engine started.")
        return {"status": "Trading engine started."}
    return {"status": "Trading engine is already running."}

@app.post("/engine/stop")
def stop_engine():
    if trading_system and trading_system.is_running():
        trading_system.stop()
        print("Trading engine stopped.")
        return {"status": "Trading engine stopped."}
    return {"status": "Trading engine is not running."}

@app.get("/engine/status")
def get_engine_status():
    is_running = trading_system.is_running() if trading_system else False
    return {"isRunning": is_running}

# Data Endpoints

@app.get("/")
def read_root():
    return {"message": "Trading Engine API is running."}

@app.get("/symbols")
def get_symbols():
    return {"symbols": trading_system.get_all_symbols() if trading_system else []}

@app.get("/metrics")
def get_system_metrics():
    cpp_metrics = trading_system.get_system_metrics() if trading_system else None
    if cpp_metrics is None:
        return {"ordersProcessed": 0, "avgLatency": 0.0, "activeOrders": 0, "eventQueueDepth": 0, "throughput": 0.0}
    return {
        "ordersProcessed": cpp_metrics.orders_processed,
        "avgLatency": cpp_metrics.avg_latency_ms,
        "activeOrders": cpp_metrics.active_orders,
        "eventQueueDepth": cpp_metrics.event_queue_depth,
        "throughput": cpp_metrics.throughput,
    }

@app.get("/market_snapshot")
def get_market_snapshot_data(symbol: str = "ETH-USD-PERP"):
    cpp_snapshot = trading_system.get_market_snapshot(symbol) if trading_system else None
    if cpp_snapshot is None:
        return {
            "bestBid": 0,
            "bestAsk": 0,
            "lastPrice": 0,
            "lastVolume": 0,
            "timestamp": int(time.time() * 1000)
        }
    return {
        "bestBid": cpp_snapshot.best_bid,
        "bestAsk": cpp_snapshot.best_ask,
        "lastPrice": cpp_snapshot.last_trade_price,
        "lastVolume": cpp_snapshot.last_trade_quantity,
        "timestamp": int(time.time() * 1000)
    }

@app.get("/trader/default_id")
def get_default_trader_id():
    return {"traderId": default_trader_id}

@app.get("/portfolio/{trader_id}")
def get_portfolio_snapshot_data(trader_id: int):
    if not trading_system:
        return {"balance": 0, "positions": {}, "unrealizedPnl": 0}

    cpp_snapshot = trading_system.get_portfolio_snapshot(trader_id)
    if cpp_snapshot is None:
        return {
            "balance": 0,
            "positions": {},
            "unrealizedPnl": 0
        }

    symbols = trading_system.get_all_symbols()
    positions_with_symbols = {}
    for symbol_id, quantity in cpp_snapshot.positions.items():
        if 0 <= symbol_id < len(symbols):
            positions_with_symbols[symbols[symbol_id]] = quantity
            
    # NOTE: unrealizedPnl is a placeholder, a full implementation would
    # require fetching current market prices to calculate this.
    return {
        "balance": cpp_snapshot.balance,
        "positions": positions_with_symbols,
        "unrealizedPnl": random.randint(-500, 500) # Placeholder with random data for visuals
    }