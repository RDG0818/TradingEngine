import trading_core
import time
from portfolio import Portfolio
from data_handler import CSVDataHandler, data_file_path
from strategy import MovingAverageCrossoverStrategy

trader_ids = set() 

class MarketSimulator:
    def __init__(self, engine, symbol: str):
        self.engine = engine
        self.symbol = symbol
        self.resting_bid_id = None
        self.resting_ask_id = None
        self.bid_trader_id = 998
        self.ask_trader_id = 999
        
    def create_market_for_bar(self, bar):
        bid_price_str = f"{bar['low']:.2f}"
        ask_price_str = f"{bar['high']:.2f}"
        
        print(f"SIM: Creating market -> BID @ {bid_price_str}, ASK @ {ask_price_str}")
        
        bid_order = trading_core.LimitOrder(
            self.symbol, 0, trading_core.OrderType.LIMIT, trading_core.Side.BUY,
            bid_price_str, 1000, self.bid_trader_id
        )
        self.resting_bid_id = self.engine.submit_order(bid_order)

        ask_order = trading_core.LimitOrder(
            self.symbol, 0, trading_core.OrderType.LIMIT, trading_core.Side.SELL,
            ask_price_str, 1000, self.ask_trader_id
        )
        self.resting_ask_id = self.engine.submit_order(ask_order)
        
    def cleanup_market(self):
        if self.resting_bid_id is not None:
           print(f"SIM: Cleaning up previous BID (ID: {self.resting_bid_id})") 
           self.engine.cancel_order(self.resting_bid_id)
           self.resting_bid_id = None

        if self.resting_ask_id is not None:
            print(f"SIM: Cleaning up previous ASK (ID: {self.resting_ask_id})")
            self.engine.cancel_order(self.resting_ask_id)
            self.resting_ask_id = None 

if __name__ == "__main__":
    print("Starting Trading System Backtest")

    dispatcher = trading_core.EventDispatcher()
    orderbook = trading_core.OrderBook()
    engine = trading_core.MatchingEngine(orderbook, dispatcher)

    portfolio = Portfolio(cash="10000.00", trader_id=1)
    data_handler = CSVDataHandler(csv_path=data_file_path, symbol="AAPL")

    market_sim = MarketSimulator(engine=engine, symbol="AAPL")
    strategy = MovingAverageCrossoverStrategy(engine, portfolio, "AAPL", 5, 50)

    dispatcher.subscribe_trade_executed(portfolio.on_fill)
    dispatcher.subscribe_market_data(portfolio.on_market_data)

    engine.start()

    for bar_data in data_handler.stream_bars():
        timestamp, bar = bar_data
        print(f"\nProcessing {timestamp} | Portfolio Value: ${portfolio.value:.2f}")
        
        market_sim.cleanup_market()
        market_sim.create_market_for_bar(bar)

        strategy.on_bar(bar)
        
        market_event = trading_core.MarketDataEvent()
        market_event.symbol = "AAPL" 
        market_event.last_price = int(bar['close'] * 100)
        portfolio.on_market_data(market_event)

        time.sleep(0.002)

    print("\n--- Backtest Complete ---")
    
    market_sim.cleanup_market()
    time.sleep(0.002)
    engine.stop()

    final_value = portfolio.value
    pnl = final_value - 10000.0
    print(f"Final Portfolio Value: ${final_value:,.2f}")
    print(f"Total Profit/Loss: ${pnl:,.2f}")
    print(f"Total Trades Executed: {portfolio.trade_count}")
    print(f"Final Holdings: {portfolio.holdings}")