// include/exchange_events.h
#pragma once
#include "order.h"
#include "order_book.h"

// Published when a fill occurs (partial or full).
struct FillEvent {
    Fill fill;
};

// Published after every order book change.
struct BookUpdateEvent {
    BookSnapshot snapshot;
};

// Published when an order is accepted into the book.
struct OrderAcceptedEvent {
    OrderId order_id;
    TraderId trader_id;
};

// Published when an order is rejected.
struct OrderRejectedEvent {
    OrderId order_id;
    TraderId trader_id;
    std::string reason;
};

// Published when an order is cancelled.
struct OrderCancelledEvent {
    OrderId order_id;
    TraderId trader_id;
};
