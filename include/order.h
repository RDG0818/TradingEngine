// include/order.h

#ifndef TRADINGENGINE_INCLUDE_ORDER_H_
#define TRADINGENGINE_INCLUDE_ORDER_H_

#include <chrono>
#include <string>
#include <utility>

#include "utils.h"

// Abstract class for other Order types

// TODO: Consider using std::variant for order types instead of polymorphism
//                    for potential performance gains in high-frequency scenarios.
class Order {

public:

  Order(SymbolID symbol_id,
      OrderID order_id,
      OrderType order_type,
      Side side,
      Quantity quantity,
      TraderID trader_id,
      TimeInForce tif = TimeInForce::GTC)
    : timestamp_(std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now())),
      order_id_(order_id),
      symbol_id_(symbol_id),
      quantity_(quantity),
      trader_id_(trader_id),
      order_type_(order_type),
      side_(side),
      tif_(tif) {
        LOG_DEBUG("Order created: id=" << order_id_ << ", type=" << ToString(order_type_)
                  << ", side=" << ToString(side_) << ", qty=" << quantity_);
      }

  virtual ~Order() = default;

  // Accessors
  SymbolID get_symbol_id() const {return symbol_id_;}
  OrderID get_order_id() const {return order_id_;}
  OrderType get_order_type() const {return order_type_;}
  OrderStatus get_order_status() const {return order_status_;}
  Side get_side() const {return side_;}
  Quantity get_quantity() const {return quantity_;}
  TraderID get_trader_id() const {return trader_id_;}
  Timestamp get_timestamp() const {return timestamp_;}
  TimeInForce get_time_in_force() const {return tif_;}
  virtual Price get_price() const = 0;
  virtual Price get_stop_price() const = 0;

  // Mutators
  void set_order_status(OrderStatus status) {
    LOG_DEBUG("Order status change: id=" << order_id_ << ", from=" << ToString(order_status_)
              << ", to=" << ToString(status));
    order_status_ = status;
  }
  // TODO: Revisit immutability of Order. Consider removing set_quantity
  //               and managing open quantity exclusively within the OrderBook
  //               via a LiveOrder wrapper, treating the Order object as immutable
  //               original intent.
  void set_quantity(Quantity q) {quantity_ = q;}

private:

  Timestamp timestamp_;
  OrderID order_id_;
  SymbolID symbol_id_;
  Quantity quantity_;
  TraderID trader_id_;
  OrderType order_type_;
  OrderStatus order_status_ = OrderStatus::NEW;
  Side side_;
  TimeInForce tif_;

};

class LimitOrder : public Order {

public:

  LimitOrder(SymbolID symbol_id,
      OrderID order_id,
      Side side,
      Price price,
      Quantity quantity,
      TraderID trader_id,
      TimeInForce tif = TimeInForce::GTC)
    : Order(symbol_id,
            order_id,
            OrderType::LIMIT,
            side,
            quantity,
            trader_id,
            tif),
      price_(price) {}

  Price get_price() const override {return price_;}
  Price get_stop_price() const override { return 0; }

private:

  Price price_;

};

class StopLimitOrder : public Order {

public:

  StopLimitOrder(SymbolID symbol_id,
      OrderID order_id,
      Side side,
      Quantity quantity,
      TraderID trader_id,
      Price stop_price,
      Price limit_price,
      TimeInForce tif = TimeInForce::GTC)
    : Order(symbol_id,
            order_id,
            OrderType::STOP_LIMIT,
            side,
            quantity,
            trader_id,
            tif),
      stop_price_(stop_price), limit_price_(limit_price) {}

  Price get_stop_price() const override {return stop_price_;}
  Price get_price() const override {return limit_price_;}

private:

  Price stop_price_;
  Price limit_price_;

};

class MarketOrder : public Order {

public:

  MarketOrder(
      SymbolID symbol_id,
      OrderID order_id,
      Side side,
      Quantity quantity,
      TraderID trader_id)
    : Order(symbol_id,
            order_id,
            OrderType::MARKET,
            side,
            quantity,
            trader_id) {}

  Price get_price() const override { return 0; }
  Price get_stop_price() const override { return 0; }

};

class StopMarketOrder : public Order {

public:

  StopMarketOrder(SymbolID symbol_id,
      OrderID order_id,
      Side side,
      Quantity quantity,
      TraderID trader_id,
      Price stop_price)
    : Order(symbol_id,
            order_id,
            OrderType::STOP_MARKET,
            side,
            quantity,
            trader_id),
      stop_price_(stop_price) {}

  Price get_price() const override { return 0; }
  Price get_stop_price() const override {return stop_price_;}

private:

  Price stop_price_;

};

#endif  // TRADINGENGINE_INCLUDE_ORDER_H_
