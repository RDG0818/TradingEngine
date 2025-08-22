import trading_core
from dataclasses import dataclass
import pandas as pd
from typing import Optional, Hashable

def validate_price(price: str):
    if '.' in price:
        decimals = price.split('.')[1]
        if len(decimals) > 2:
            raise ValueError("Too many decimal places")
    return float(price)

@dataclass
class Position:
    symbol: str
    quantity: int
    cost_basis: float
    market_value: float 

class Portfolio:
    def __init__(self, cash: str, trader_id: int, holdings: dict = {}, commissions: float = 0):
        self.cash: float = validate_price(cash) 
        self.holdings: dict = holdings
        self.trader_id = trader_id 
        self.value = self.cash + sum(pos.market_value for pos in self.holdings.values())
        self.commissions = commissions
        self.trade_count = 0
        self.current_bar_timestamp: Optional[Hashable] = None
        self.history = []
        self.trades = []

    def get_position(self, symbol: str) -> Position | None:
        return self.holdings.get(symbol)
    
    def unrealized_pnl(self) -> dict[str, float]:
        pnl = {}
        for symbol, position in self.holdings.items():
            total_cost = position.cost_basis * position.quantity
            market_value = position.market_value
            pnl[symbol] = market_value - total_cost
        return pnl
    
    def get_total_unrealized_pnl(self) -> float:
        return sum(self.unrealized_pnl().values())

    def on_fill(self, event: trading_core.TradeExecutedEvent):
        my_side = None
        if event.aggressing_trader_id == self.trader_id:
            my_side = event.aggressing_side
        elif event.resting_trader_id == self.trader_id:
            my_side = trading_core.Side.BUY if event.aggressing_side == trading_core.Side.SELL else trading_core.Side.SELL
        else:
            return
        self.trade_count += 1

        fill_price = float(event.price) / 100
        fill_quantity = event.quantity
        symbol = event.symbol
        fill_direction = "BUY" if my_side == trading_core.Side.BUY else "SELL"
        
        self.trades.append({
            'timestamp': self.current_bar_timestamp,
            'symbol': symbol,
            'side': fill_direction,
            'price': fill_price,
            'quantity': fill_quantity
        })
        
        print(f"Portfolio {self.trader_id} processing fill: {fill_direction} {fill_quantity} {symbol} @ {fill_price}")

        signed_quantity = fill_quantity if fill_direction == "BUY" else -fill_quantity
        transaction_cost = signed_quantity * fill_price
        self.cash -= transaction_cost
        
        current_pos = self.holdings.get(
            symbol, 
            Position(symbol=symbol, quantity=0, cost_basis=0.0, market_value=0.0)
        )

        old_quantity = current_pos.quantity
        new_quantity = old_quantity + signed_quantity

        if new_quantity == 0:
            if symbol in self.holdings:
                del self.holdings[symbol]
            return

        if fill_direction == "BUY":
            old_total_cost = current_pos.cost_basis * old_quantity
            fill_total_cost = fill_quantity * fill_price
            new_cost_basis = (old_total_cost + fill_total_cost) / new_quantity
            current_pos.cost_basis = new_cost_basis

        current_pos.quantity = new_quantity
        current_pos.market_value = new_quantity * fill_price
        self.holdings[symbol] = current_pos
        self.value = self.cash + sum(pos.market_value for pos in self.holdings.values())

    def on_market_data(self, event: trading_core.MarketDataEvent):
        if event.symbol in self.holdings:
            position = self.holdings[event.symbol]
            old_market_value = position.market_value
            new_market_value = position.quantity * event.last_price / 100.0
            position.market_value = new_market_value
            self.value += new_market_value - old_market_value
            
    def log_state(self, timestamp):
        self.history.append({
            'timestamp': timestamp,
            'value': self.value
        })

    def save_results(self, history_filepath="csv/backtest_history.csv", trades_filepath="csv/backtest_trades.csv"):
        pd.DataFrame(self.history).to_csv(history_filepath, index=False)
        pd.DataFrame(self.trades).to_csv(trades_filepath, index=False)