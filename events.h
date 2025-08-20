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
    
    Milliseconds timestamp = std::chrono::duration_cast<Milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
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
    uint64_t timestamp;
};