#pragma once
#include <string>
#include <chrono>
#include <utility>
#include "utils.h"

// Abstract class for other Order types

class Order {
private:
    Timestamp timestamp;
    OrderID orderID;
    SymbolID symbolID;
    Quantity quantity;
    TraderID traderID;
    OrderType orderType;
    OrderStatus orderStatus = OrderStatus::NEW;
    Side side;
    TimeInForce tif;

public:
    Order(SymbolID symbolID, OrderID orderID, OrderType orderType, Side side, Quantity quantity, TraderID traderID, TimeInForce tif = TimeInForce::GTC)
        : timestamp(std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now())),
          orderID(orderID),
          symbolID(symbolID),
          quantity(quantity),
          traderID(traderID),
          orderType(orderType),
          side(side),
          tif(tif) {
    }

    virtual ~Order() = default;

    SymbolID getSymbolID() const {
        return symbolID;
    }
    virtual Price getPrice() const { return 0; }
    OrderID getOrderID() const {
        return orderID;
    }
    OrderType getOrderType() const {
        return orderType;
    }
    OrderStatus getOrderStatus() const {
        return orderStatus;
    }
    Side getSide() const {
        return side;
    }
    Quantity getQuantity() const {
        return quantity;
    }
    TraderID getTraderID() const {
        return traderID;
    }
    Timestamp getTimestamp() const {
        return timestamp;
    }
    TimeInForce getTimeInForce() const {
        return tif;
    }

    void setOrderStatus(OrderStatus status) {
        orderStatus = status;
    }

    void setQuantity(Quantity q) {
        quantity = q;
    }

    // NOTE: This should only be used for internal setup or testing
    void setOrderID(OrderID id) {
        orderID = id;
    }
};


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

class StopMarketOrder : public Order {
private:

    Price stopPrice;

public:
    StopMarketOrder(SymbolID symbolID, OrderID orderID, Side side, Quantity quantity, TraderID traderID, Price stopPrice)
        : Order(symbolID, orderID, OrderType::STOP_MARKET, side, quantity, traderID), stopPrice(stopPrice) {}

    Price getStopPrice() const {
        return stopPrice;
    }

    Price getPrice() const override { return 0; }
};
