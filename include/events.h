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
    Timestamp get_timestamp() const { return timestamp_; }

protected:
    Timestamp timestamp_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
};

struct OrderRejectedEvent : public BaseEvent {
  OrderID order_id;
  TraderID trader_id;
  RejectionReason reason;
  std::string message;

  OrderRejectedEvent(OrderID p_order_id, TraderID p_trader_id, RejectionReason p_reason, std::string p_message)
    : order_id(p_order_id), trader_id(p_trader_id), reason(p_reason), message(std::move(p_message)) {}
};

struct OrderAcceptedEvent : public BaseEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Side side;
  Price price;
  Quantity quantity;

  OrderAcceptedEvent(SymbolID p_symbol_id, OrderID p_order_id, TraderID p_trader_id, Side p_side, Price p_price, Quantity p_quantity)
    : symbol_id(p_symbol_id), order_id(p_order_id), trader_id(p_trader_id), side(p_side), price(p_price), quantity(p_quantity) {}
};

struct OrderCancelledEvent : public BaseEvent {
  SymbolID symbol_id;
  OrderID order_id;
  TraderID trader_id;
  Quantity quantity;

  OrderCancelledEvent(SymbolID p_symbol_id, OrderID p_order_id, TraderID p_trader_id, Quantity p_quantity)
    : symbol_id(p_symbol_id), order_id(p_order_id), trader_id(p_trader_id), quantity(p_quantity) {}
};

// Level 1/2 Feed information

struct BookUpdateEvent : public BaseEvent {
  SymbolID symbol_id;
  Price best_bid_price;
  Quantity best_bid_quantity;
  Price best_ask_price;
  Quantity best_ask_quantity;

  BookUpdateEvent(SymbolID p_symbol_id, Price p_best_bid_price, Quantity p_best_bid_quantity, Price p_best_ask_price, Quantity p_best_ask_quantity)
    : symbol_id(p_symbol_id), best_bid_price(p_best_bid_price), best_bid_quantity(p_best_bid_quantity), best_ask_price(p_best_ask_price), best_ask_quantity(p_best_ask_quantity) {}
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

  TradeExecutedEvent(SymbolID p_symbol_id, Price p_price, Quantity p_quantity,
                     OrderID p_aggressing_order_id, TraderID p_aggressing_trader_id, Side p_aggressing_side, Quantity p_aggressing_order_remaining_quantity,
                     OrderID p_resting_order_id, TraderID p_resting_trader_id, Quantity p_resting_order_remaining_quantity)
    : symbol_id(p_symbol_id), price(p_price), quantity(p_quantity),
      aggressing_order_id(p_aggressing_order_id), aggressing_trader_id(p_aggressing_trader_id), aggressing_side(p_aggressing_side), aggressing_order_remaining_quantity(p_aggressing_order_remaining_quantity),
      resting_order_id(p_resting_order_id), resting_trader_id(p_resting_trader_id), resting_order_remaining_quantity(p_resting_order_remaining_quantity) {}
};

// New Events for Order State
struct OrderPartiallyFilledEvent : public BaseEvent {
    OrderID order_id;
    TraderID trader_id;
    Quantity quantity_filled;
    Quantity remaining_quantity;
    Price last_fill_price;

    OrderPartiallyFilledEvent(OrderID p_order_id, TraderID p_trader_id, Quantity p_quantity_filled, Quantity p_remaining_quantity, Price p_last_fill_price)
        : order_id(p_order_id), trader_id(p_trader_id), quantity_filled(p_quantity_filled), remaining_quantity(p_remaining_quantity), last_fill_price(p_last_fill_price) {}
};

struct OrderFilledEvent : public BaseEvent {
    OrderID order_id;
    TraderID trader_id;
    Price last_fill_price; // Price of the trade that caused the order to be fully filled

    OrderFilledEvent(OrderID p_order_id, TraderID p_trader_id, Price p_last_fill_price)
        : order_id(p_order_id), trader_id(p_trader_id), last_fill_price(p_last_fill_price) {}
};

#endif  // TRADINGENGINE_INCLUDE_EVENTS_H_