#pragma once
#include "types.h"
#include "order.h"
#include <chrono>

struct TradeExecutedEvent {
    SymbolID symbolID;

    Price price;
    Quantity quantity;

    OrderID aggressingOrderID;
    TraderID aggressingTraderID;
    Side aggressingSide;
    Quantity aggressingRemainingQuantity;
    
    OrderID restingOrderID;
    TraderID restingTraderID;
    Quantity restingRemainingQuantity;
    
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderAcceptedEvent {
    OrderID orderID;
    Price price;
    Quantity quantity;
};

struct OrderCancelledEvent {
    OrderID orderID;
    Quantity quantity;
};

struct MarketDataEvent {
    SymbolID symbolID;
    Price last_price; 
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};