import build.trading_engine_py as te
import time

# 1. Initialize TradingSystem
print("Initializing trading system...")
system = te.TradingSystem(10, ["GEM"])

# 2. Start the system
print("Starting system...")
system.start()
time.sleep(0.1) # Give the system a moment to start

# 3. Create a portfolio
print("Creating portfolio...")
trader_id = system.create_portfolio(100000_0000) # Starting balance (using integer for price)
print(f"Portfolio created with Trader ID: {trader_id}")

# 4. Define a limit order
print("Submitting a limit order...")
order_params = te.RawOrderParams()
order_params.symbol = "GEM"
order_params.order_type = te.OrderType.LIMIT
order_params.side = te.Side.BUY
order_params.price = "150.0000" # Price as a string
order_params.quantity = 10
order_params.trader_id = trader_id

# 5. Submit the order
order_id = system.submit_order(trader_id, order_params)
print(f"Submitted order with Order ID: {order_id}")

time.sleep(0.1) # Wait for order to be processed

# 6. Get market snapshot
print("\n--- Market Snapshot for GEM ---")
market_snapshot = system.get_market_snapshot("GEM")
if market_snapshot:
    print(f"  Best Bid: {market_snapshot.best_bid_quantity} @ {market_snapshot.best_bid}")
    print(f"  Best Ask: {market_snapshot.best_ask_quantity} @ {market_snapshot.best_ask}")
    print(f"  Last Trade: {market_snapshot.last_trade_quantity} @ {market_snapshot.last_trade_price}")
else:
    print("  No market snapshot available.")

# 7. Get portfolio snapshot
print(f"\n--- Portfolio Snapshot for Trader {trader_id} ---")
portfolio_snapshot = system.get_portfolio_snapshot(trader_id)
if portfolio_snapshot:
    print(f"  Balance: {portfolio_snapshot.balance}")
    print("  Positions:")
    for symbol_id, quantity in portfolio_snapshot.positions.items():
        # In a real scenario, you'd map symbol_id back to a string
        print(f"    Symbol ID {symbol_id}: {quantity}")
    print(f"  Trade History Count: {len(portfolio_snapshot.trade_history)}")
else:
    print("  No portfolio snapshot available.")

# 8. Stop the system
print("\nStopping system...")
system.stop()
print("System stopped.")
