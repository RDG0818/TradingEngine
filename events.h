#pragma once
#include "types.h"
#include "order.h"
#include <chrono>

struct TradeExecutedEvent {
    str symbol;

    Price price;
    Quantity quantity;

    OrderID aggressingOrderID;
    TraderID aggressingTraderID;
    Side aggressingSide;
    Quantity aggressingRemainingQuantity;
    
    OrderID restingOrderID;
    TraderID restingTraderID;
    Quantity restingRemainingQuantity;
    
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
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
    std::string symbol;
    Price last_price; 
    Timestamp timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
};