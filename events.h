#pragma once
#include "types.h"

struct TradeExecutedEvent {
    OrderID aggressingOrderID;
    OrderID restingOrderID;
    Price price;
    Quantity quantity;
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