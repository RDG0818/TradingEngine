#pragma once
#include "order.h"

class LimitOrder : public Order {

private:

    Price price;

public:

    LimitOrder(SymbolID symbolID, OrderID orderID, Side side, Price price, Quantity quantity, TraderID traderID, TimeInForce tif = TimeInForce::GTC)
        : Order(symbolID, orderID, OrderType::LIMIT, side, quantity, traderID, tif), price(price) {}

    Price getPrice() const override {
        return price;
    }

};