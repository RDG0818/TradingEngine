// include/events.h

#ifndef TRADINGENGINE_INCLUDE_EVENTS_H_
#define TRADINGENGINE_INCLUDE_EVENTS_H_

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

inline std::string ToString(RejectionReason reason) {
    switch (reason) {
        case RejectionReason::INVALID_SYMBOL: return "INVALID_SYMBOL";
        case RejectionReason::INVALID_PRICE: return "INVALID_PRICE";
        case RejectionReason::INVALID_QUANTITY: return "INVALID_QUANTITY";
        case RejectionReason::UNSUPPORTED_ORDER_TYPE: return "UNSUPPORTED_ORDER_TYPE";
        case RejectionReason::ORDER_ID_ALREADY_EXISTS: return "ORDER_ID_ALREADY_EXISTS";
        case RejectionReason::INSUFFICIENT_FUNDS: return "INSUFFICIENT_FUNDS";
        case RejectionReason::OTHER: return "OTHER";
    }
    return "UNKNOWN_REJECTION_REASON";
}

// Base class for all events
class BaseEvent {
public:
    virtual ~BaseEvent() = default;
    Timestamp get_timestamp() const { return timestam; }

protected:
    Timestamp timestam = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderRejectedEvent : public BaseEvent {
  OrderID order_id;
  TraderID trader_id;
  RejectionReason reason;
  std::string message;

  OrderRejectedEvent(OrderID order_id, TraderID trader_id, RejectionReason reason, std::string message)
    : order_id(order_id), trader_id(trader_id), reason(reason), message(std::move(message)) {}
};

struct OrderAcceptedEvent : public BaseEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Side side;
  Price price;
  Quantity quantity;

  OrderAcceptedEvent(SymbolID symbol_id, OrderID order_id, TraderID trader_id, Side side, Price price, Quantity quantity)
    : symbol_id(symbol_id), order_id(order_id), trader_id(trader_id), side(side), price(price), quantity(quantity) {}
};

struct OrderCancelledEvent : public BaseEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Quantity quantity;

  OrderCancelledEvent(SymbolID symbol_id, OrderID order_id, TraderID trader_id, Quantity quantity)
    : symbol_id(symbol_id), order_id(order_id), trader_id(trader_id), quantity(quantity) {}
};

// Level 1/2 Feed information

struct BookUpdateEvent : public BaseEvent {
  SymbolID symbol_id;
  Price best_bid_price;
  Quantity best_bid_quantity;
  Price best_ask_price;
  Quantity best_ask_quantity;

  BookUpdateEvent(SymbolID symbol_id, Price best_bid_price, Quantity best_bid_quantity, Price best_ask_price, Quantity best_ask_quantity)
    : symbol_id(symbol_id), best_bid_price(best_bid_price), best_bid_quantity(best_bid_quantity), best_ask_price(best_ask_price), best_ask_quantity(best_ask_quantity) {}
};

// Level 3 Feed information

struct TradeExecutedEvent : public BaseEvent {
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

  TradeExecutedEvent(SymbolID symbol_id, Price price, Quantity quantity,
                     OrderID aggressing_order_id, TraderID aggressing_trader_id, Side aggressing_side, Quantity aggressing_order_remaining_quantity,
                     OrderID resting_order_id, TraderID resting_trader_id, Quantity resting_order_remaining_quantity)
    : symbol_id(symbol_id), price(price), quantity(quantity),
      aggressing_order_id(aggressing_order_id), aggressing_trader_id(aggressing_trader_id), aggressing_side(aggressing_side), aggressing_order_remaining_quantity(aggressing_order_remaining_quantity),
      resting_order_id(resting_order_id), resting_trader_id(resting_trader_id), resting_order_remaining_quantity(resting_order_remaining_quantity) {}
};

// New Events for Order State
struct OrderPartiallyFilledEvent : public BaseEvent {
    OrderID order_id;
    TraderID trader_id;
    Quantity quantity_filled;
    Quantity remaining_quantity;
    Price last_fill_price;

    OrderPartiallyFilledEvent(OrderID order_id, TraderID trader_id, Quantity quantity_filled, Quantity remaining_quantity, Price last_fill_price)
        : order_id(order_id), trader_id(trader_id), quantity_filled(quantity_filled), remaining_quantity(remaining_quantity), last_fill_price(last_fill_price) {}
};

struct OrderFilledEvent : public BaseEvent {
    OrderID order_id;
    TraderID trader_id;
    Price last_fill_price; // Price of the trade that caused the order to be fully filled

    OrderFilledEvent(OrderID order_id, TraderID trader_id, Price last_fill_price)
        : order_id(order_id), trader_id(trader_id), last_fill_price(last_fill_price) {}
};

#endif  // TRADINGENGINE_INCLUDE_EVENTS_H_