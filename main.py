import trading_core
import time
from portfolio import Portfolio

trader_ids = set() 

class CSVDataHandler:
    """A placeholder for your data handler that reads from a CSV."""
    def __init__(self, csv_filepath: str):
        # In the real version, you'll load the CSV here (e.g., with pandas)
        print(f"Data handler initialized for {csv_filepath}")
        self._data = [{'timestamp': '2025-08-18 10:00:00', 'close': 150.00},
                      {'timestamp': '2025-08-18 10:01:00', 'close': 150.50},
                      {'timestamp': '2025-08-18 10:02:00', 'close': 151.00}] # Dummy data
        self._data_stream = iter(self._data)

    def stream_bars(self):
        """A generator that yields one data bar at a time."""
        return self._data_stream

class MarketMakerStrategy:
    """
    A strategy that simulates a realistic trade by having two different
    traders interact. Our portfolio will be Trader 1.
    """
    def __init__(self, engine, my_trader_id: int):
        self.engine = engine
        self.my_trader_id = my_trader_id
        # Define a separate ID for the other trader
        self.opponent_trader_id = my_trader_id + 1 
        self.orders_sent = 0

    def on_bar(self, bar):
        """Called for each new market data bar."""
        price_str = f"{bar['close']:.2f}"
        
        # On the first bar, have the OPPONENT submit a SELL order.
        # This order will rest on the book, waiting for us.
        if self.orders_sent == 0:
            print("  -> Opponent is submitting a resting SELL order.")
            sell_order = trading_core.LimitOrder(
                "AAPL", 0, trading_core.OrderType.LIMIT, trading_core.Side.SELL,
                price_str, 10, self.opponent_trader_id # Use opponent's ID
            )
            self.engine.submit_order(sell_order)
            self.orders_sent += 1
        
        # On the second bar, OUR STRATEGY submits a crossing BUY order.
        elif self.orders_sent == 1:
            print(f"  -> Our strategy (Trader {self.my_trader_id}) is submitting a BUY order.")
            buy_order = trading_core.LimitOrder(
                "AAPL", 0, trading_core.OrderType.LIMIT, trading_core.Side.BUY,
                price_str, 10, self.my_trader_id # Use our ID
            )
            self.engine.submit_order(buy_order)
            self.orders_sent += 1

if __name__ == "__main__":
    print("Starting Trading System Backtest")

    dispatcher = trading_core.EventDispatcher()
    orderbook = trading_core.OrderBook()
    engine = trading_core.MatchingEngine(orderbook, dispatcher)

    portfolio = Portfolio(cash="10000.00", trader_id=1)
    data_handler = CSVDataHandler(csv_filepath="./data/AAPL.csv")
    strategy = MarketMakerStrategy(engine=engine, my_trader_id=1)

    dispatcher.subscribe_trade_executed(portfolio.on_fill)
    dispatcher.subscribe_market_data(portfolio.on_market_data)

    engine.start()

    for bar in data_handler.stream_bars():
        print(f"\nProcessing {bar['timestamp']} | Portfolio Value: {portfolio.value:.2f}")
        
        market_event = trading_core.MarketDataEvent()
        market_event.symbol = "AAPL" 
        market_event.last_price = int(bar['close'] * 100)
        portfolio.on_market_data(market_event)

        strategy.on_bar(bar)
        time.sleep(0.01)

    print("\n--- Backtest Complete ---")
    
    engine.stop()

    final_value = portfolio.value
    pnl = final_value - 10000.0
    print(f"Final Portfolio Value: ${final_value:,.2f}")
    print(f"Total Profit/Loss: ${pnl:,.2f}")
    print(f"Total Trades Executed: {len(portfolio.holdings)}")