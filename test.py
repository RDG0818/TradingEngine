import build.trading_engine_py as te

limit_order = te.LimitOrder(
symbol_id=1,
order_id=1001,
side=te.Side.BUY,
price=150,
quantity=10,
trader_id=1
)

print(f"Created Order: ID={limit_order.get_order_id}, Type={limit_order.get_order_type}, Price={limit_order.get_price}")