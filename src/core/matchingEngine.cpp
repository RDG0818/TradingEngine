// src/core/matchingEngine.cpp

#include <iostream>
#include <variant>

#include "matchingEngine.h"
#include "orderFactory.h"
#include "symbolRegistry.h"

MatchingEngine::MatchingEngine(EventDispatcher& event_dispatcher)
  : event_dispatcher_(event_dispatcher), next_order_id_(1) {}

MatchingEngine::~MatchingEngine() {
  stop();
}

OrderID MatchingEngine::submit_order(const RawOrderParams& params) {
  OrderID id = next_order_id_.fetch_add(1);

  try {
    SymbolID symbolID = SymbolRegistry::get_instance().get_id(params.symbol);
    {
      std::lock_guard<std::mutex> lock(order_map_mutex_);
      order_id_to_symbol_[id] = symbolID;
    }

    auto order = order_factory::create_order(params, id);
    event_queue_.push(std::move(order));
  } 
  catch (const InvalidPriceException& e) {
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_PRICE, e.what()));
  } 
  catch (const InvalidQuantityException& e) {
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_QUANTITY, e.what()));
  } 
  catch (const UnsupportedOrderTypeException& e) {
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::UNSUPPORTED_ORDER_TYPE, e.what()));
  } 
  catch (const std::invalid_argument& e) {
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::OTHER, e.what()));
  }
  return id;
}

void MatchingEngine::cancel_order(OrderID orderID) {
  event_queue_.push(orderID);
}

void MatchingEngine::start() {
  running_ = true;
  worker_thread_ = std::thread(&MatchingEngine::run_loop, this);
}

void MatchingEngine::stop() {
  running_ = false;
  event_queue_.push(OrderID{0}); // wake up the thread
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

OrderBook* MatchingEngine::get_or_create_order_book(SymbolID symbolID) {
  {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    auto it = books_.find(symbolID);
    if (it != books_.end()) {
      return it->second.get();
    }
  }

  std::unique_lock<std::shared_mutex> lock(books_mutex_);
  auto it = books_.find(symbolID);
  if (it != books_.end()) {
    return it->second.get();
  }

  books_[symbolID] = std::make_unique<OrderBook>();
  return books_[symbolID].get();
}

OrderBook* MatchingEngine::get_book(SymbolID symbolID) {
  std::shared_lock<std::shared_mutex> lock(books_mutex_);
  auto it = books_.find(symbolID);
  if (it == books_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void MatchingEngine::run_loop() {
  while (running_) {
    EngineEvent event = event_queue_.pop();
    std::visit([this](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<Order>>) {
        if (arg) process_order_submission(std::move(arg));
      } 
      else if constexpr (std::is_same_v<T, OrderID>) {
        if (arg != 0) process_order_cancellation(arg);
      }
    }, std::move(event));
  }
}

void MatchingEngine::process_order_submission(std::unique_ptr<Order> order) {
  if (order->get_quantity() == 0) {
    event_dispatcher_.publish(OrderRejectedEvent(order->get_order_id(), order->get_trader_id(), RejectionReason::INVALID_QUANTITY, "Quantity must be positive."));
    return;
  }

  if (order->get_order_type() == OrderType::STOP_MARKET) {
    process_stop_market_order(std::unique_ptr<StopMarketOrder>(static_cast<StopMarketOrder*>(order.release())));
    return;
  } 
  else if (order->get_order_type() == OrderType::STOP_LIMIT) {
    process_stop_limit_order(std::unique_ptr<StopLimitOrder>(static_cast<StopLimitOrder*>(order.release())));
    return;
  }

  OrderBook* book = get_or_create_order_book(order->get_symbol_id());

  // Level 1/2 Feed Dissemination
    
  auto prevBestBid = book->get_best_bid();
  auto prevBestAsk = book->get_best_ask();

  match_order(order.get(), *book);

  auto currentBestBid = book->get_best_bid();
  auto currentBestAsk = book->get_best_ask();

  bool bidChanged = (prevBestBid.has_value() != currentBestBid.has_value()) || 
                    (currentBestBid.has_value() && (prevBestBid->price != currentBestBid->price || 
                    prevBestBid->quantity != currentBestBid->quantity));

  bool askChanged = (prevBestAsk.has_value() != currentBestAsk.has_value()) || 
                      (currentBestAsk.has_value() && (prevBestAsk->price != currentBestAsk->price || 
                      prevBestAsk->quantity != currentBestAsk->quantity));

  if (bidChanged || askChanged) {
    event_dispatcher_.publish(BookUpdateEvent(
      order->get_symbol_id(),
      currentBestBid.value_or(MarketData{0, 0}).price,
      currentBestBid.value_or(MarketData{0, 0}).quantity,
      currentBestAsk.value_or(MarketData{0, 0}).price,
      currentBestAsk.value_or(MarketData{0, 0}).quantity
    ));
  }

  if (order->get_quantity() > 0) {
    if (order->get_time_in_force() == TimeInForce::IOC || order->get_time_in_force() == TimeInForce::FOK) {
      order->set_order_status(OrderStatus::CANCELLED);
      event_dispatcher_.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
    } 
    else if (order->get_order_type() == OrderType::LIMIT) {
      place_resting_limit_order(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release())), *book); 
    } 
    else {
      order->set_order_status(OrderStatus::CANCELLED);
      event_dispatcher_.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
    }
  }
}

void MatchingEngine::process_stop_market_order(std::unique_ptr<StopMarketOrder> order) {
  const OrderID order_id = order->get_order_id();
  const Price stop_price = order->get_stop_price();
  const SymbolID symbol_id = order->get_symbol_id();

  // Store the ID in the correct stop order list, segregated by symbol
  {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->get_side() == Side::BUY) {
      stop_orders_by_symbol_[symbol_id].buy_stops[stop_price].push_back(order_id);
    } else {
      stop_orders_by_symbol_[symbol_id].sell_stops[stop_price].push_back(order_id);
    }
  }

  // Move the order into the ownership map to keep it alive
  {
    std::lock_guard<std::mutex> lock(untriggered_orders_mutex_);
    untriggered_orders_[order_id] = std::move(order);
  }
}

void MatchingEngine::process_stop_limit_order(std::unique_ptr<StopLimitOrder> order) {
  const OrderID order_id = order->get_order_id();
  const Price stop_price = order->get_stop_price();
  const SymbolID symbol_id = order->get_symbol_id();

  // Store the ID in the correct stop order list, segregated by symbol
  {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->get_side() == Side::BUY) {
      stop_orders_by_symbol_[symbol_id].buy_stops[stop_price].push_back(order_id);
    } else {
      stop_orders_by_symbol_[symbol_id].sell_stops[stop_price].push_back(order_id);
    }
  }

  // Move the order into the ownership map to keep it alive
  {
    std::lock_guard<std::mutex> lock(untriggered_orders_mutex_);
    untriggered_orders_[order_id] = std::move(order);
  }
}

void MatchingEngine::process_order_cancellation(OrderID order_id) {
  SymbolID symbol_id;
  {
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    auto it = order_id_to_symbol_.find(order_id);
    if (it == order_id_to_symbol_.end()) {
      // No known symbol for this order ID, cannot proceed.
      // Could be an already-filled order, or an invalid ID.
      return;
    }
    symbol_id = it->second;
  }

  OrderBook* book = get_book(symbol_id);
  if (book && book->get_order(order_id)) {
    Order* order = book->get_order(order_id);
    TraderID trader_id = order->get_trader_id();
    Quantity quantity = order->get_quantity();
    try {
      book->cancel_order(order_id);
      event_dispatcher_.publish(OrderCancelledEvent(symbol_id, order_id, trader_id, quantity));
      std::lock_guard<std::mutex> lock(order_map_mutex_);
      order_id_to_symbol_.erase(order_id);
    } 
    catch (const std::invalid_argument& e) {
            // TODO: add logging for failed cancellation
    }
  }
  else {
    // If not in the book, it might be an untriggered stop order.
    std::lock_guard<std::mutex> lock(untriggered_orders_mutex_);
    auto it = untriggered_orders_.find(order_id);
    if (it != untriggered_orders_.end()) {
      std::unique_ptr<Order>& order = it->second;
      TraderID trader_id = order->get_trader_id();
      Quantity quantity = order->get_quantity();
      // Remove from the price-level stop map
      {
        std::lock_guard<std::mutex> stop_lock(stop_orders_mutex_);
        auto& stop_orders = stop_orders_by_symbol_[symbol_id];
        Price stop_price = 0;

        if (order->get_order_type() == OrderType::STOP_MARKET) {
          stop_price = static_cast<StopMarketOrder*>(order.get())->get_stop_price();
        } 
        else if (order->get_order_type() == OrderType::STOP_LIMIT) {
          stop_price = static_cast<StopLimitOrder*>(order.get())->get_stop_price();
        }

        auto& stop_map = (order->get_side() == Side::BUY) ? stop_orders.buy_stops : stop_orders.sell_stops;
        if(stop_map.count(stop_price)) {
          auto& stop_list = stop_map.at(stop_price);
          stop_list.remove(order_id);
          if (stop_list.empty()) {
            stop_map.erase(stop_price);
          }
        }
      }
    // Erase from the untriggered map (which also deletes the unique_ptr)
    untriggered_orders_.erase(it);

    event_dispatcher_.publish(OrderCancelledEvent(symbol_id, order_id, trader_id, quantity));

    std::lock_guard<std::mutex> symbol_lock(order_map_mutex_);
    order_id_to_symbol_.erase(order_id);
    }
  }
}

void MatchingEngine::match_order(Order* incomingOrder, OrderBook& book) {
  if (incomingOrder->get_time_in_force() == TimeInForce::FOK) {
    Quantity availableQuantityAtBestPrice = 0;
    if (incomingOrder->get_side() == Side::BUY) {
      if (book.get_best_ask().has_value()) {
        auto bestAsk = book.get_best_ask().value();
        if (incomingOrder->get_order_type() == OrderType::MARKET || incomingOrder->get_price() >= bestAsk.price) {
          availableQuantityAtBestPrice = bestAsk.quantity;
        }
      }
    } 
    else { // incomingOrder->get_side() == Side::SELL
      if (book.get_best_bid().has_value()) {
        auto bestBid = book.get_best_bid().value();
        if (incomingOrder->get_order_type() == OrderType::MARKET || incomingOrder->get_price() <= bestBid.price) {
          availableQuantityAtBestPrice = bestBid.quantity;
        }
      }
    }
    if (incomingOrder->get_quantity() > availableQuantityAtBestPrice) {
      return; // FOK order cannot be fully filled at best price, so it's rejected
    }
  }

  while (incomingOrder->get_quantity() > 0) {
    std::optional<MarketData> bestOpposingLevel = (incomingOrder->get_side() == Side::BUY) ? book.get_best_ask() : book.get_best_bid();

    if (!bestOpposingLevel) break;

    if (incomingOrder->get_order_type() == OrderType::LIMIT) {
      auto limitOrder = static_cast<LimitOrder*>(incomingOrder);
      Price limitPrice = limitOrder->get_price();
      Price bestOpposingPrice = bestOpposingLevel->price;

      if (incomingOrder->get_side() == Side::BUY && limitPrice < bestOpposingPrice) break;
      if (incomingOrder->get_side() == Side::SELL && limitPrice > bestOpposingPrice) break;
    }
        
    // Use the new callback-based forEachOrderAtPrice
    bool continueMatchingAtPriceLevel = book.for_each_order_at_price(bestOpposingLevel->price,
      incomingOrder->get_side() == Side::BUY ? Side::SELL : Side::BUY,
      [&](OrderID restingOrderID) {
        Order* restingOrder = book.get_order(restingOrderID); 
        if (!restingOrder) return true; // Continue iteration if order not found

        // Self Match Prevention
        if (restingOrder->get_trader_id() == incomingOrder->get_trader_id()) {
          event_dispatcher_.publish(OrderCancelledEvent(
                                    restingOrder->get_symbol_id(),
                                    restingOrderID,
                                    restingOrder->get_trader_id(),
                                    restingOrder->get_quantity()));

          book.cancel_order(restingOrderID);
          return true;
        }

        Quantity tradeQuantity = std::min(incomingOrder->get_quantity(), restingOrder->get_quantity());
        Price tradePrice = restingOrder->get_price();

        create_trade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

        incomingOrder->set_quantity(incomingOrder->get_quantity() - tradeQuantity);
        book.reduce_order_quantity(restingOrderID, tradeQuantity); 

        if (incomingOrder->get_quantity() == 0) {
          return false; // Aggressor fully filled, stop iterating orders at this price level
        }
        return true; // Continue iteration
      });

    // If forEachOrderAtPrice returned false because the incoming order was fully filled,
    // the while loop condition will handle breaking out. No explicit 'break' needed here.
  }
}

void MatchingEngine::place_resting_limit_order(std::unique_ptr<LimitOrder> order, OrderBook& book) {
  order->set_order_status(OrderStatus::ACCEPTED);
  event_dispatcher_.publish(OrderAcceptedEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_side(), order->get_price(), order->get_quantity()));
  book.add_order(std::move(order));
}

void MatchingEngine::create_trade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {
  Quantity aggressorRemaining = aggressor->get_quantity() - tradeQuantity;
  Quantity restingRemaining = resting->get_quantity() - tradeQuantity;
  aggressor->set_order_status(aggressorRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
  resting->set_order_status(restingRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);

  event_dispatcher_.publish(TradeExecutedEvent(aggressor->get_symbol_id(), tradePrice, tradeQuantity, 
                            aggressor->get_order_id(), aggressor->get_trader_id(), aggressor->get_side(), aggressor->get_quantity(), 
                            resting->get_order_id(), resting->get_trader_id(), resting->get_quantity()));

  if (aggressorRemaining == 0) {
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    order_id_to_symbol_.erase(aggressor->get_order_id());
  }
  if (restingRemaining == 0) {
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    order_id_to_symbol_.erase(resting->get_order_id());
  }

  trigger_stop_orders(aggressor->get_symbol_id(), tradePrice);
}

void MatchingEngine::trigger_stop_orders(SymbolID symbol_id, Price last_trade_price) {
  std::vector<OrderID> triggered_ids;
    
  {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    auto symbol_stops_it = stop_orders_by_symbol_.find(symbol_id);
    if (symbol_stops_it == stop_orders_by_symbol_.end()) {
      return; // No stop orders for this symbol
    }
    auto& stop_orders = symbol_stops_it->second;

    for (auto it = stop_orders.buy_stops.begin(); it != stop_orders.buy_stops.end() && it->first <= last_trade_price; ) {
      triggered_ids.insert(triggered_ids.end(), it->second.begin(), it->second.end());
      it = stop_orders.buy_stops.erase(it);
    }

    for (auto it = stop_orders.sell_stops.begin(); it != stop_orders.sell_stops.end() && it->first >= last_trade_price; ) {
      triggered_ids.insert(triggered_ids.end(), it->second.begin(), it->second.end());
      it = stop_orders.sell_stops.erase(it);
    }
  }

  // Activate the triggered orders 
  if (!triggered_ids.empty()) {
    std::lock_guard<std::mutex> untriggered_lock(untriggered_orders_mutex_);
    for (OrderID id : triggered_ids) {
      auto order_it = untriggered_orders_.find(id);
      if (order_it != untriggered_orders_.end()) {
        std::unique_ptr<Order> triggered_order = std::move(order_it->second);
        untriggered_orders_.erase(order_it);
              
        if (triggered_order->get_order_type() == OrderType::STOP_MARKET) {
          auto* stop_order = static_cast<StopMarketOrder*>(triggered_order.get());
          event_queue_.push(std::make_unique<MarketOrder>(
                      stop_order->get_symbol_id(), stop_order->get_order_id(), 
                      stop_order->get_side(), stop_order->get_quantity(), stop_order->get_trader_id()));
        } 
        else if (triggered_order->get_order_type() == OrderType::STOP_LIMIT) {
          auto* stop_order = static_cast<StopLimitOrder*>(triggered_order.get());
          event_queue_.push(std::make_unique<LimitOrder>(
                      stop_order->get_symbol_id(), stop_order->get_order_id(), stop_order->get_side(), 
                      stop_order->get_price(), stop_order->get_quantity(), stop_order->get_trader_id()));
        }
      }
    }
  }
}

std::optional<MarketData> MatchingEngine::get_best_ask(SymbolID symbolID) const {
  std::shared_lock<std::shared_mutex> lock(books_mutex_);
  auto it = books_.find(symbolID);
  if (it != books_.end()) {
    return it->second->get_best_ask();
  }
  return std::nullopt;
}

std::optional<MarketData> MatchingEngine::get_best_bid(SymbolID symbolID) const {
  std::shared_lock<std::shared_mutex> lock(books_mutex_);
  auto it = books_.find(symbolID);
  if (it != books_.end()) {
    return it->second->get_best_bid();
  }
  return std::nullopt;
}

void MatchingEngine::print_top_of_book(std::string symbol, int num_price_levels) {
  SymbolID symbolID = SymbolRegistry::get_instance().get_id(symbol);
  OrderBook* ob = get_book(symbolID);
  std::cout << std::endl << "\x1b[1m" << "\033[1;33m" << symbol << "\033[0m" 
  << " Top of Orderbook: " << "\x1b[0m" << std::endl;
  if (ob != nullptr) ob->print(num_price_levels);
}
