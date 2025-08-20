#pragma once
#include <string>
#include <chrono>
#include <utility>
#include <stdexcept>
#include "types.h"
using str = std::string;

enum class Side { BUY, SELL };

inline Side getOppositeSide(Side side) {
    return (side == Side::BUY) ? Side::SELL : Side::BUY;
};

enum class OrderType {
    LIMIT,
    MARKET
};

enum class OrderStatus {
    NEW,
    ACCEPTED,
    REJECTED,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED
};

class Order {
    private:
        str symbol;
        OrderID orderID;
        OrderType orderType;
        OrderStatus orderStatus = OrderStatus::NEW;
        Side side;
        Quantity quantity;
        TraderID traderID;
        Milliseconds timestamp = std::chrono::duration_cast<Milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
    public:
        virtual ~Order() {}

        const str& getSymbol() const {
            return symbol;
        }    
        void setSymbol(str s) {
            this->symbol = s;
        }
        OrderID getOrderID() {
            return orderID;
        };
        void setOrderID(OrderID o) {
           this->orderID = o; 
        }
        OrderType getOrderType() {
            return orderType;
        }
        void setOrderType(OrderType orderType) {
            this->orderType = orderType;
        }
        OrderStatus getOrderStatus() {
            return orderStatus;
        }
        void setOrderStatus(OrderStatus os) {
            this->orderStatus = os;
        }
        Side getSide() {
            return side;
        }
        void setSide(Side s) {
            this->side = s;
        }
        Quantity getQuantity() {
            return quantity;
        }
        void setQuantity(Quantity q) {
            this->quantity = q;
        }
        TraderID getTraderID() {
            return traderID;
        } 
        void setTraderID(TraderID t) {
            this->traderID = t;
        }
        Milliseconds getTimestamp() {
            return timestamp;
        }
       };