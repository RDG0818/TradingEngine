#pragma once
#include "order.h"

class StopMarketOrder : public Order {
private:
    Price stopPrice;

public:
    StopMarketOrder(SymbolID symbolID, OrderID orderID, Side side, Price stopPrice, Quantity quantity, TraderID traderID)
        : Order(symbolID, orderID, OrderType::STOP_MARKET, side, quantity, traderID), stopPrice(stopPrice) {}

    Price getStopPrice() const {
        return stopPrice;
    }
};