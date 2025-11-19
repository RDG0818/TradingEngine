#pragma once
#include "types.h"
#include "order.h"
#include <chrono>
#include <string>

struct TradeExecutedEvent {
    SymbolID symbolID;

    Price price;
    Quantity quantity;

    OrderID aggressingOrderID;
    TraderID aggressingTraderID;
    Side aggressingSide;
    Quantity aggressingOrderRemainingQuantity;
    
    OrderID restingOrderID;
    TraderID restingTraderID;
    Quantity restingOrderRemainingQuantity;
    
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderAcceptedEvent {
    SymbolID symbolID;
    OrderID orderID;
    TraderID traderID;
    Side side;
    Price price;
    Quantity quantity;
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

enum class RejectionReason {
    INVALID_SYMBOL,
    INVALID_PRICE,
    INVALID_QUANTITY,
    UNSUPPORTED_ORDER_TYPE,
    ORDER_ID_ALREADY_EXISTS,
    INSUFFICIENT_FUNDS, // Example for future risk checks
    OTHER
};

struct OrderRejectedEvent {
    OrderID orderID;
    TraderID traderID;
    RejectionReason reason;
    std::string message;
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderCancelledEvent {
    SymbolID symbolID;
    OrderID orderID;
    TraderID traderID;
    Quantity quantity;
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct MarketDataEvent {
    SymbolID symbolID;
    Price last_price; 
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};