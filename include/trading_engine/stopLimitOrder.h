#pragma once
#include "order.h"

class StopLimitOrder : public Order {
private:
    Price price;
    Price stopPrice;

public:
    StopLimitOrder(SymbolID symbolID, OrderID orderID, Side side, Price price, Price stopPrice, Quantity quantity, TraderID traderID)
        : Order(symbolID, orderID, OrderType::STOP_LIMIT, side, quantity, traderID), price(price), stopPrice(stopPrice) {}

    Price getPrice() const override {
        return price;
    }

    Price getStopPrice() const {
        return stopPrice;
    }
};