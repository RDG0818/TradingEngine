#pragma once
#include "utils.h"
#include "order.h"
#include <chrono>
#include <string>

enum class RejectionReason {
    INVALID_SYMBOL,
    INVALID_PRICE,
    INVALID_QUANTITY,
    UNSUPPORTED_ORDER_TYPE,
    ORDER_ID_ALREADY_EXISTS,
    INSUFFICIENT_FUNDS, 
    OTHER
};

struct OrderRejectedEvent {
    OrderID orderID;
    TraderID traderID;
    RejectionReason reason;
    std::string message;
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

struct OrderCancelledEvent {
    SymbolID symbolID;
    OrderID orderID;
    TraderID traderID;
    Quantity quantity;
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

// Level 1/2 Feed information

struct BookUpdateEvent {
    SymbolID symbolID;
    Price bestBidPrice;
    Quantity bestBidQty;
    Price bestAskPrice;
    Quantity bestAskQty;
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

// Level 3 Feed information

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