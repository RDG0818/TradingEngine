#pragma once
#include <string>
#include <chrono>
#include <utility>
#include "types.h"

class Order {
private:
    // Reordered for better memory alignment
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