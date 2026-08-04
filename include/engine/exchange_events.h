#pragma once
#include <chrono>
#include "core/order.h"
#include "engine/order_book.h"

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
    std::chrono::steady_clock::time_point submit_tp;  // wall clock at dequeue time
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
