import trading_core
from collections import deque
import numpy as np

class MovingAverageCrossoverStrategy:
    def __init__(self, engine, portfolio, symbol: str, short_window=10, long_window=30):
        self.engine = engine
        self.portfolio = portfolio
        self.symbol = symbol

        self.short_window = short_window
        self.long_window = long_window

        self.prices = deque(maxlen=long_window)
        self.short_ma = 0
        self.long_ma = 0

        self.position_state = "FLAT" # or "LONG"

    def on_bar(self, bar):
        close_price = bar['close']
        self.prices.append(close_price)

        if len(self.prices) < self.long_window: return

        price_array = np.array(self.prices)
        self.short_ma = np.mean(price_array[-self.short_window:])
        self.long_ma = np.mean(price_array)

        print(f"STRATEGY: Short MA: {self.short_ma:.2f}, Long MA: {self.long_ma:.2f}")

        if self.short_ma > self.long_ma and self.position_state == "FLAT":
            print(">>> STRATEGY: BUY SIGNAL DETECTED <<<")
            self.position_state = "LONG"

            buy_order = trading_core.MarketOrder(
                self.symbol, 0, trading_core.OrderType.MARKET, trading_core.Side.BUY,
                10,
                self.portfolio.trader_id
            )
            self.engine.submit_order(buy_order)
        elif self.short_ma < self.long_ma and self.position_state == "LONG":
            print(">>> STRATEGY: SELL SIGNAL DETECTED <<<")
            self.position_state = "FLAT"
            sell_order = trading_core.MarketOrder(
                self.symbol, 0, trading_core.OrderType.MARKET, trading_core.Side.SELL,
                10,
                self.portfolio.trader_id
            )  
            self.engine.submit_order(sell_order)