// include/trading_engine/order.h
#pragma once

#include <chrono>
#include <string>
#include <utility>

#include "utils.h"

// Abstract class for other Order types

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
      tif_(tif) {}

  virtual ~Order() = default;

  // Accessors
  SymbolID getSymbolID() const {return symbol_id_;}
  OrderID getOrderID() const {return order_id_;}
  OrderType getOrderType() const {return order_type_;}
  OrderStatus getOrderStatus() const {return order_status_;}
  Side getSide() const {return side_;}
  Quantity getQuantity() const {return quantity_;}
  TraderID getTraderID() const {return trader_id_;}
  Timestamp getTimestamp() const {return timestamp_;}
  TimeInForce getTimeInForce() const {return tif_;}
  virtual Price getPrice() const { return 0; }
  virtual Price getStopPrice() const { return 0; }

  // Mutators
  void setOrderStatus(OrderStatus status) {order_status_ = status;}
  void setQuantity(Quantity q) {quantity_ = q;}
  void setOrderID(OrderID id) {order_id_ = id;} // Should only be used for internal setup or testing

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

  Price getPrice() const override {return price_;}
  Price getStopPrice() const override { return 0; }

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

  Price getStopPrice() const {return stop_price_;}
  Price getPrice() const override {return limit_price_;}

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

  Price getPrice() const override { return 0; }
  Price getStopPrice() const override { return 0; }

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

  Price getPrice() const override { return 0; }
  Price getStopPrice() const {return stop_price_;}

private:

  Price stop_price_;

};
