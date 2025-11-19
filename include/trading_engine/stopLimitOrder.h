#pragma once
#include "order.h"
#include "types.h"

class StopLimitOrder : public Order {
private:
    Price stopPrice;
    Price limitPrice;

public:
    StopLimitOrder(SymbolID symbolID, OrderID orderID, Side side, Quantity quantity, TraderID traderID, Price stopPrice, Price limitPrice, TimeInForce tif = TimeInForce::GTC)
        : Order(symbolID, orderID, OrderType::STOP_LIMIT, side, quantity, traderID, tif), stopPrice(stopPrice), limitPrice(limitPrice) {}

    Price getStopPrice() const {
        return stopPrice;
    }

    Price getPrice() const override {
        return limitPrice;
    }
};
