#pragma once
#include "order.h"
#include "types.h"

class StopMarketOrder : public Order {
private:
    Price stopPrice;

public:
    StopMarketOrder(SymbolID symbolID, OrderID orderID, Side side, Quantity quantity, TraderID traderID, Price stopPrice)
        : Order(symbolID, orderID, OrderType::STOP_MARKET, side, quantity, traderID), stopPrice(stopPrice) {}

    Price getStopPrice() const {
        return stopPrice;
    }
};
