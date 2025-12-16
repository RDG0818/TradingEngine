// include/trading_engine/events.h
#pragma once

#include <chrono>
#include <string>

#include "utils.h"
#include "order.h"

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
  OrderID order_id;
  TraderID trader_id;
  RejectionReason reason;
  std::string message;
  Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderAcceptedEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Side side;
  Price price;
  Quantity quantity;
  Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderCancelledEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Quantity quantity;
  Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

// Level 1/2 Feed information

struct BookUpdateEvent {
  SymbolID symbol_id;
  Price best_bid_price;
  Quantity best_bid_quantity;
  Price best_ask_price;
  Quantity best_ask_quantity;
  Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

// Level 3 Feed information

struct TradeExecutedEvent {
  SymbolID symbol_id;

  Price price;
  Quantity quantity;

  OrderID aggressing_order_id;
  TraderID aggressing_trader_id;
  Side aggressing_side;
  Quantity aggressing_order_remaining_quantity;

  OrderID resting_order_id;
  TraderID resting_trader_id;
  Quantity resting_order_remaining_quantity;

  Timestamp timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};