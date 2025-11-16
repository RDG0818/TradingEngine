#pragma once
#include "order.h"
#include "types.h"

class MarketOrder : public Order {
public:
    MarketOrder(
        SymbolID symbolID,
        OrderID orderID,
        Side side,
        Quantity quantity,
        TraderID traderID
    ) : Order(symbolID, orderID, OrderType::MARKET, side, quantity, traderID) {}

    Price getPrice() const override { return 0; }
};
