// src/core/portfolio.cpp
#include "portfolio.h"

Price Portfolio::get_balance() const {
  return balance_;
}

Quantity Portfolio::get_position(SymbolID symbol_id) const {
  auto it = assets_.find(symbol_id);
  if (it != assets_.end()) {
    return it->second;
  }
  else {
    return 0;
  }
}

const std::deque<std::shared_ptr<Order>>& Portfolio::get_order_history() const {
  return order_history_;
}

const std::deque<TradeExecutedEvent>& Portfolio::get_trade_history() const {
  return trade_history_;
};

const std::map<SymbolID, Quantity>& Portfolio::get_all_positions() const {
  return assets_;
}

bool Portfolio::can_submit_order(const std::shared_ptr<Order>& order) const {
  if (order->get_quantity() <= 0) {
    return false;
  }

  if (order->get_side() == Side::SELL) {
    return get_position(order->get_symbol_id()) >= order->get_quantity();
  }

  Price check_price = 0;

  switch (order->get_order_type()) {
    case OrderType::LIMIT:
      check_price = order->get_price();
      break;

    case OrderType::STOP_LIMIT:
      check_price = order->get_price();
      break;

    case OrderType::STOP_MARKET:
      check_price = order->get_stop_price();
      break;

    case OrderType::MARKET: {
      auto best_ask = engine_.get_best_ask(order->get_symbol_id());
      if (!best_ask.has_value()) return false; // no sellers
      check_price = static_cast<Price>(best_ask->price * 1.05); // assume 5% "slippage" buffer
      break;
    }

    default:
      return false;
  }

  if(check_price <= 0) {
    return false;
  }

  return (check_price * order->get_quantity()) <= balance_;
}

void Portfolio::on_trade_executed(const TradeExecutedEvent& trade, const std::shared_ptr<Order>& order) {
  std::optional<Side> my_side;
  if (trader_id_ == trade.aggressing_trader_id) {
    my_side = trade.aggressing_side;
  } else if (trader_id_ == trade.resting_trader_id) {
    // The resting side is the opposite of the aggressor's
    my_side = (trade.aggressing_side == Side::BUY) ? Side::SELL : Side::BUY;
  }

  if (!my_side.has_value()) {
    // TODO: This case should ideally not happen if events are routed correctly.
    // Log an error here if it does.
    return;
  }

  Price balance_change = trade.price * trade.quantity;

  if (my_side.value() == Side::BUY) {
    balance_ -= balance_change;
    assets_[trade.symbol_id] += trade.quantity;
  } else { // Side::SELL
    balance_ += balance_change;
    assets_[trade.symbol_id] -= trade.quantity;
  }
  
  
  trade_history_.push_front(trade); 
  if (trade_history_.size() > MAX_TRADE_HISTORY_SIZE) {
    trade_history_.pop_back();
  }

  order_history_.push_front(order); 
  if (order_history_.size() > MAX_ORDER_HISTORY_SIZE) {
    order_history_.pop_back();
  }
}

void Portfolio::reset_balance() {
  balance_ = starting_balance_;
  for (auto order : order_history_) {
    if (order->get_order_status() != OrderStatus::FILLED && 
      order->get_order_status() != OrderStatus::CANCELLED &&
      order->get_order_status() != OrderStatus::REJECTED) {
      engine_.cancel_order(order->get_order_id());
    }
  }
  assets_.clear();
  // TODO: Add PnL reset here
}

TraderID Portfolio::get_trader_id() const {
  return trader_id_;
}



