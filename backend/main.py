from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from typing import Optional, Dict
from pydantic import BaseModel
import trading_engine_py
import random
import time

# --- Global State ---
trading_system: Optional[trading_engine_py.TradingSystem] = None
default_trader_id: Optional[int] = None
managed_traders: Dict[str, Dict] = {}

SYMBOLS = ["SPY", "QQQ"]

# --- Type Conversions ---
trader_type_to_str = {
    trading_engine_py.TraderType.RANDOM_MARKET: "Random Market",
    trading_engine_py.TraderType.RANDOM_LIMIT: "Random Limit",
    trading_engine_py.TraderType.MARKET_MAKER: "Market Maker",
}
str_to_trader_type = {v: k for k, v in trader_type_to_str.items()}


# --- App Lifecycle ---
@asynccontextmanager
async def lifespan(app: FastAPI):
    global trading_system, default_trader_id
    trading_system = trading_engine_py.TradingSystem(100, SYMBOLS)
    print("TradingSystem singleton instance created.")

    if trading_system:
        default_trader_id = trading_system.create_portfolio(100000000)
        print(f"Default portfolio created with trader_id: {default_trader_id}")

    trading_system.start()
    print("TradingSystem singleton instance started.")

    yield
    if trading_system and trading_system.is_running():
        trading_system.stop()
        print("TradingSystem singleton instance stopped.")

app = FastAPI(lifespan=lifespan)

# --- CORS ---
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost", "http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- Pydantic Models ---
class ResetRequest(BaseModel):
    trader_id: int

class TraderData(BaseModel):
    name: str
    type: str
    parameters: Dict[str, float]

class TraderParameters(BaseModel):
    parameters: Dict[str, float]

# --- Endpoints ---

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
def get_market_snapshot_data(symbol: str = ""):
    target_symbol = symbol if symbol else SYMBOLS[0]
    cpp_snapshot = trading_system.get_market_snapshot(target_symbol) if trading_system else None
    if cpp_snapshot is None:
        return { "bestBid": 0, "bestAsk": 0, "lastPrice": 0, "lastVolume": 0, "timestamp": int(time.time() * 1000) }
    return {
        "bestBid": round(cpp_snapshot.best_bid/10000, 2),
        "bestAsk": round(cpp_snapshot.best_ask/10000, 2),
        "lastPrice": round(cpp_snapshot.last_trade_price/10000, 2),
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
        return { "balance": 0, "positions": {}, "unrealizedPnl": 0 }

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

@app.post("/portfolio/reset")
def reset_portfolio(req: ResetRequest):
    if trading_system:
        trading_system.reset_balance(req.trader_id)
        print(f"Portfolio reset for trader_id: {req.trader_id}")
        return {"status": f"Portfolio for trader {req.trader_id} has been reset."}
    return {"status": "Trading system not available."}


# Automated Traders
@app.get("/traders")
def get_traders():
    if not trading_system:
        return {"traders": []}
    
    traders_info = trading_system.get_all_traders()
    trader_list = []

    for info in traders_info:
        params = trading_system.get_trader_parameters(info.name) or {}
        is_active = trading_system.is_trader_active(info.name)
        metrics = trading_system.get_trader_metrics(info.id)
        
        trader_list.append({
            "id": info.name,
            "name": info.name,
            "type": trader_type_to_str.get(info.type, "Unknown"),
            "status": "running" if is_active else "idle",
            "parameters": params,
            "pnl": random.randint(-100, 200), # Placeholder
            "latency": metrics.avg_latency_ms,
            "ops": metrics.orders_per_second,
            "recentHistory": [random.randint(0, 100) for _ in range(30)] if is_active else [] # Placeholder
        })
    return {"traders": trader_list}

@app.post("/traders")
def create_trader(trader_data: TraderData):
    global managed_traders
    if not trading_system:
        raise HTTPException(status_code=503, detail="Trading system not available")

    name = trader_data.name
    params = trader_data.parameters
    max_qty = int(params.get("max_quantity", 10))

    # Check if a trader with the same name exists in the engine, not just our managed list
    existing_traders = trading_system.get_all_traders()
    if any(t.name == name for t in existing_traders):
        raise HTTPException(status_code=409, detail=f"Trader with name '{name}' already exists in the engine.")

    try:
        if trader_data.type == "Random Market":
            trading_system.add_random_market_trader(name, params["lambda"], max_qty)
        elif trader_data.type == "Random Limit":
            trading_system.add_random_limit_trader(name, params["lambda"], max_qty, params["normal_distribution_variance"])
        elif trader_data.type == "Market Maker":
            trading_system.add_market_maker_trader(name, params["mu"], params["sigma"], params["spread"], max_qty, {"SPY": 540.0})
        else:
            raise HTTPException(status_code=400, detail="Unknown trader type")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to create trader in engine: {e}")

    # After adding, it should be stopped by default
    trading_system.stop_trader(name)
    
    managed_traders[name] = trader_data.dict()
    # Return a response consistent with the GET /traders endpoint for a single trader
    new_trader_response = {
        "id": name, "name": name, "type": trader_data.type, 
        "status": "idle", "parameters": params,
        "pnl": 0, "latency": 0, "ops": 0, "recentHistory": []
    }
    return new_trader_response

@app.put("/traders/{name}")
def update_trader_parameters(name: str, data: TraderParameters):
    if not trading_system:
        raise HTTPException(status_code=503, detail="Trading system not available")

    if not trading_system.set_trader_parameters(name, data.parameters):
        raise HTTPException(status_code=500, detail="Failed to set trader parameters in engine.")

    if name in managed_traders:
        managed_traders[name]["parameters"] = data.parameters
    
    return {"status": "success"}


@app.delete("/traders/{name}")
def delete_trader(name: str):
    global managed_traders
    if not trading_system:
        raise HTTPException(status_code=503, detail="Trading system not available")
    
    if trading_system.remove_trader(name):
        if name in managed_traders:
            del managed_traders[name]
        return {"status": "success", "message": f"Trader '{name}' removed."}
    else:
        # Also try to remove from managed list if it exists there but not in engine
        if name in managed_traders:
            del managed_traders[name]
            return {"status": "success", "message": f"Trader '{name}' removed from managed list (was not in engine)."}
        raise HTTPException(status_code=404, detail=f"Trader '{name}' not found.")


@app.post("/traders/{name}/toggle")
def toggle_trader_status(name: str):
    if not trading_system:
        raise HTTPException(status_code=503, detail="Trading system not available")

    is_active = trading_system.is_trader_active(name)

    if is_active:
        if trading_system.stop_trader(name):
            return {"status": "stopped"}
        else:
            raise HTTPException(status_code=500, detail="Failed to stop trader in engine.")
    else:
        if trading_system.start_trader(name):
            return {"status": "started"}
        else:
            raise HTTPException(status_code=500, detail="Failed to start trader in engine.")
