#pragma once
#include "event.h"

enum class RejectionReason {
    INVALID_PRICE,
    INVALID_QUANTITY,
    UNSUPPORTED_ORDER_TYPE,
    OTHER
};

struct OrderAcceptedEvent : public Event {
    SymbolID symbolID;
    OrderID orderID;
    TraderID traderID;
    Side side;
    Price price;
    Quantity quantity;

    OrderAcceptedEvent(SymbolID symbolID, OrderID orderID, TraderID traderID, Side side, Price price, Quantity quantity)
        : symbolID(symbolID), orderID(orderID), traderID(traderID), side(side), price(price), quantity(quantity) {
        type = EventType::ORDER_ACCEPTED;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "OrderAccepted: " << orderID << " for " << symbolID << " (" << (side == Side::BUY ? "BUY" : "SELL") << " " << quantity << "@" << price << ")";
        return ss.str();
    }
};

struct OrderRejectedEvent : public Event {
    OrderID orderID;
    TraderID traderID;
    RejectionReason reason;
    std::string details;

    OrderRejectedEvent(OrderID orderID, TraderID traderID, RejectionReason reason, std::string details)
        : orderID(orderID), traderID(traderID), reason(reason), details(details) {
        type = EventType::ORDER_REJECTED;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "OrderRejected: " << orderID << " - " << details;
        return ss.str();
    }
};

struct OrderCancelledEvent : public Event {
    SymbolID symbolID;
    OrderID orderID;
    TraderID traderID;
    Quantity quantity;

    OrderCancelledEvent(SymbolID symbolID, OrderID orderID, TraderID traderID, Quantity quantity)
        : symbolID(symbolID), orderID(orderID), traderID(traderID), quantity(quantity) {
        type = EventType::ORDER_CANCELLED;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "OrderCancelled: " << orderID << " for " << symbolID;
        return ss.str();
    }
};

struct TradeExecutedEvent : public Event {
    SymbolID symbolID;
    Price tradePrice;
    Quantity tradeQuantity;
    OrderID aggressorOrderID;
    TraderID aggressorTraderID;
    Side aggressorSide;
    Quantity aggressorRemainingQuantity;
    OrderID restingOrderID;
    TraderID restingTraderID;
    Quantity restingRemainingQuantity;

    TradeExecutedEvent(SymbolID symbolID, Price tradePrice, Quantity tradeQuantity, OrderID aggressorOrderID, TraderID aggressorTraderID, Side aggressorSide, Quantity aggressorRemainingQuantity, OrderID restingOrderID, TraderID restingTraderID, Quantity restingRemainingQuantity)
        : symbolID(symbolID), tradePrice(tradePrice), tradeQuantity(tradeQuantity), aggressorOrderID(aggressorOrderID), aggressorTraderID(aggressorTraderID), aggressorSide(aggressorSide), aggressorRemainingQuantity(aggressorRemainingQuantity), restingOrderID(restingOrderID), restingTraderID(restingTraderID), restingRemainingQuantity(restingRemainingQuantity) {
        type = EventType::TRADE_EXECUTED;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "TradeExecuted: " << tradeQuantity << " of " << symbolID << " @ " << tradePrice
           << " (Aggressor: " << aggressorOrderID << ", Resting: " << restingOrderID << ")";
        return ss.str();
    }
};
