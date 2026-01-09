// src/core/matchingEngine.cpp

#include <iostream>
#include <variant>

#include "matchingEngine.h"
#include "orderFactory.h"
#include "symbolRegistry.h"

MatchingEngine::MatchingEngine(EventDispatcher& event_dispatcher, OrderIdGenerator& id_generator)
  : event_dispatcher_(event_dispatcher), id_generator_(id_generator) {
    LOG_DEBUG("MatchingEngine constructor");
}

MatchingEngine::~MatchingEngine() {
  LOG_DEBUG("MatchingEngine destructor");
  if (!stopped_) {
    stop();
  }
}

void MatchingEngine::stop() {
  LOG_DEBUG("MatchingEngine::stop() called");
  if (stopped_.exchange(true)) {
    LOG_DEBUG("MatchingEngine::stop() - already stopped");
    return; // Already stopped or stopping
  }
  running_ = false;
  LOG_DEBUG("MatchingEngine::stop() - waking up worker thread");
  event_queue_.push(OrderID{0}); // wake up the thread
  if (worker_thread_.joinable()) {
    LOG_DEBUG("MatchingEngine::stop() - joining worker thread");
    worker_thread_.join();
    LOG_DEBUG("MatchingEngine::stop() - worker thread joined");
  }
}

OrderID MatchingEngine::submit_order(const RawOrderParams& params) {
  OrderID id = id_generator_.new_id();
  LOG_DEBUG("submit_order: New order ID " + std::to_string(id) + " for symbol " + params.symbol);

  try {
    SymbolID symbolID = SymbolRegistry::get_instance().get_id(params.symbol);
    {
      std::lock_guard<std::mutex> lock(order_map_mutex_);
      order_id_to_symbol_[id] = symbolID;
    }

    auto order = order_factory::create_order(params, id);
    LOG_DEBUG("submit_order: Pushing order " + std::to_string(id) + " to event queue");
    event_queue_.push(std::move(order));
  } 
  catch (const InvalidPriceException& e) {
    LOG_DEBUG("submit_order: REJECTED order " + std::to_string(id) + " - InvalidPriceException: " + e.what());
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_PRICE, e.what()));
  } 
  catch (const InvalidQuantityException& e) {
    LOG_DEBUG("submit_order: REJECTED order " + std::to_string(id) + " - InvalidQuantityException: " + e.what());
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_QUANTITY, e.what()));
  } 
  catch (const UnsupportedOrderTypeException& e) {
    LOG_DEBUG("submit_order: REJECTED order " + std::to_string(id) + " - UnsupportedOrderTypeException: " + e.what());
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::UNSUPPORTED_ORDER_TYPE, e.what()));
  } 
  catch (const std::invalid_argument& e) {
    LOG_DEBUG("submit_order: REJECTED order " + std::to_string(id) + " - invalid_argument: " + e.what());
    event_dispatcher_.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::OTHER, e.what()));
  }
  return id;
}

void MatchingEngine::submit_order(std::shared_ptr<Order> order) {
  LOG_DEBUG("submit_order: Pushing pre-created order " + std::to_string(order->get_order_id()) + " to event queue");
  {
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    order_id_to_symbol_[order->get_order_id()] = order->get_symbol_id();
  }
  event_queue_.push(std::move(order));
}

void MatchingEngine::cancel_order(OrderID orderID) {
  LOG_DEBUG("cancel_order: Pushing cancellation for order " + std::to_string(orderID) + " to event queue");
  event_queue_.push(orderID);
}

void MatchingEngine::start() {
  LOG_DEBUG("MatchingEngine::start() called");
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    LOG_DEBUG("MatchingEngine::start() - already running");
    return;
  }
  stopped_ = false;
  worker_thread_ = std::thread(&MatchingEngine::run_loop, this);
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
  LOG_DEBUG("MatchingEngine::run_loop() started");
  while (running_) {
    LOG_DEBUG("MatchingEngine::run_loop() - waiting for event");
    EngineEvent event = event_queue_.pop();
    LOG_DEBUG("MatchingEngine::run_loop() - popped event");

    std::visit([this](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::shared_ptr<Order>>) {
        if (arg) {
            LOG_DEBUG("Dispatching order submission: ID " + std::to_string(arg->get_order_id()));
            process_order_submission(std::move(arg));
        }
      } 
      else if constexpr (std::is_same_v<T, OrderID>) {
        if (arg != 0) {
            LOG_DEBUG("Dispatching order cancellation: ID " + std::to_string(arg));
            process_order_cancellation(arg);
        } else {
            LOG_DEBUG("Received sentinel stop event");
        }
      }
    }, std::move(event));
  }
  LOG_DEBUG("MatchingEngine::run_loop() finished");
}

void MatchingEngine::process_order_submission(std::shared_ptr<Order> order) {
  LOG_DEBUG("process_order_submission: Processing order " + std::to_string(order->get_order_id()));
  if (order->get_quantity() == 0) {
    LOG_DEBUG("process_order_submission: REJECTED order " + std::to_string(order->get_order_id()) + " - zero quantity");
    event_dispatcher_.publish(OrderRejectedEvent(order->get_order_id(), order->get_trader_id(), RejectionReason::INVALID_QUANTITY, "Quantity must be positive."));
    return;
  }

  if (order->get_order_type() == OrderType::STOP_MARKET) {
    LOG_DEBUG("process_order_submission: Identified as STOP_MARKET, delegating. ID " + std::to_string(order->get_order_id()));
    process_stop_market_order(std::static_pointer_cast<StopMarketOrder>(order));
    return;
  } 
  else if (order->get_order_type() == OrderType::STOP_LIMIT) {
    LOG_DEBUG("process_order_submission: Identified as STOP_LIMIT, delegating. ID " + std::to_string(order->get_order_id()));
    process_stop_limit_order(std::static_pointer_cast<StopLimitOrder>(order));
    return;
  }

  LOG_DEBUG("process_order_submission: Getting/creating book for order " + std::to_string(order->get_order_id()));
  OrderBook* book = get_or_create_order_book(order->get_symbol_id());

  // Capture book state before any changes
  auto prevBestBid = book->get_best_bid();
  auto prevBestAsk = book->get_best_ask();

  LOG_DEBUG("process_order_submission: Matching order " + std::to_string(order->get_order_id()));
  match_order(order.get(), *book);
  LOG_DEBUG("process_order_submission: Finished matching order " + std::to_string(order->get_order_id()) + ", remaining qty: " + std::to_string(order->get_quantity()));

  // Process any remaining quantity
  if (order->get_quantity() > 0) {
    if (order->get_time_in_force() == TimeInForce::IOC || order->get_time_in_force() == TimeInForce::FOK) {
      LOG_DEBUG("process_order_submission: Cancelling unfilled IOC/FOK order " + std::to_string(order->get_order_id()));
      order->set_order_status(OrderStatus::CANCELLED);
      event_dispatcher_.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
    } 
    else if (order->get_order_type() == OrderType::LIMIT) {
      LOG_DEBUG("process_order_submission: Placing resting LIMIT order " + std::to_string(order->get_order_id()));
      place_resting_limit_order(std::static_pointer_cast<LimitOrder>(order), *book); 
    } 
    else { // Market orders with remaining quantity are cancelled
      LOG_DEBUG("process_order_submission: Cancelling unfilled MARKET order " + std::to_string(order->get_order_id()));
      order->set_order_status(OrderStatus::CANCELLED);
      event_dispatcher_.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
    }
  } else {
      LOG_DEBUG("process_order_submission: Order " + std::to_string(order->get_order_id()) + " fully filled.");
  }
  
  // Capture book state after all changes and publish update if needed
  auto currentBestBid = book->get_best_bid();
  auto currentBestAsk = book->get_best_ask();

  bool bidChanged = (prevBestBid.has_value() != currentBestBid.has_value()) || 
                    (prevBestBid.has_value() && currentBestBid.has_value() && (prevBestBid->price != currentBestBid->price || prevBestBid->quantity != currentBestBid->quantity));

  bool askChanged = (prevBestAsk.has_value() != currentBestAsk.has_value()) || 
                    (prevBestAsk.has_value() && currentBestAsk.has_value() && (prevBestAsk->price != currentBestAsk->price || prevBestAsk->quantity != currentBestAsk->quantity));

  if (bidChanged || askChanged) {
    event_dispatcher_.publish(BookUpdateEvent(
      order->get_symbol_id(),
      currentBestBid.value_or(MarketData{0, 0}).price,
      currentBestBid.value_or(MarketData{0, 0}).quantity,
      currentBestAsk.value_or(MarketData{0, 0}).price,
      currentBestAsk.value_or(MarketData{0, 0}).quantity
    ));
  }
}

void MatchingEngine::process_stop_market_order(std::shared_ptr<StopMarketOrder> order) {
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

void MatchingEngine::process_stop_limit_order(std::shared_ptr<StopLimitOrder> order) {
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
  LOG_DEBUG("process_order_cancellation: Processing cancellation for order " + std::to_string(order_id));
  SymbolID symbol_id;
  {
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    auto it = order_id_to_symbol_.find(order_id);
    if (it == order_id_to_symbol_.end()) {
      LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " not found in order_id_to_symbol_ map. Might be already filled/cancelled.");
      return;
    }
    symbol_id = it->second;
  }

  OrderBook* book = get_book(symbol_id);
  if (book && book->get_order(order_id)) {
    // Capture previous best bid/ask
    auto prevBestBid = book->get_best_bid();
    auto prevBestAsk = book->get_best_ask();

    LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " found in order book. Attempting cancellation.");
    Order* order = book->get_order(order_id);
    TraderID trader_id = order->get_trader_id();
    Quantity quantity = order->get_quantity();
    try {
      book->cancel_order(order_id);
      event_dispatcher_.publish(OrderCancelledEvent(symbol_id, order_id, trader_id, quantity));
      LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " successfully cancelled from book.");
      std::lock_guard<std::mutex> lock(order_map_mutex_);
      order_id_to_symbol_.erase(order_id);
    } 
    catch (const std::invalid_argument& e) {
      LOG_DEBUG("process_order_cancellation: FAILED to cancel order " + std::to_string(order_id) + " from book: " + e.what());
    }

    // Check for changes and publish BookUpdateEvent
    auto currentBestBid = book->get_best_bid();
    auto currentBestAsk = book->get_best_ask();

    bool bidChanged = (prevBestBid.has_value() != currentBestBid.has_value()) || 
                      (prevBestBid.has_value() && currentBestBid.has_value() && (prevBestBid->price != currentBestBid->price || prevBestBid->quantity != currentBestBid->quantity));

    bool askChanged = (prevBestAsk.has_value() != currentBestAsk.has_value()) || 
                        (prevBestAsk.has_value() && currentBestAsk.has_value() && (prevBestAsk->price != currentBestAsk->price || prevBestAsk->quantity != currentBestAsk->quantity));

    if (bidChanged || askChanged) {
      event_dispatcher_.publish(BookUpdateEvent(
        symbol_id,
        currentBestBid.value_or(MarketData{0, 0}).price,
        currentBestBid.value_or(MarketData{0, 0}).quantity,
        currentBestAsk.value_or(MarketData{0, 0}).price,
        currentBestAsk.value_or(MarketData{0, 0}).quantity
      ));
    }
  }
  else {
    LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " not in book, checking untriggered stop orders.");
    std::lock_guard<std::mutex> lock(untriggered_orders_mutex_);
    auto it = untriggered_orders_.find(order_id);
    if (it != untriggered_orders_.end()) {
      LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " found in untriggered orders. Cancelling.");
      std::shared_ptr<Order>& order = it->second;
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
    LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " successfully cancelled from untriggered orders.");

    std::lock_guard<std::mutex> symbol_lock(order_map_mutex_);
    order_id_to_symbol_.erase(order_id);
    } else {
        LOG_DEBUG("process_order_cancellation: Order " + std::to_string(order_id) + " not found anywhere to cancel.");
    }
  }
}

void MatchingEngine::match_order(Order* incomingOrder, OrderBook& book) {
  LOG_DEBUG("match_order: Attempting to match order " + std::to_string(incomingOrder->get_order_id()) + " with qty " + std::to_string(incomingOrder->get_quantity()));
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
      LOG_DEBUG("match_order: FOK order " + std::to_string(incomingOrder->get_order_id()) + " cannot be fully filled. Required: " + std::to_string(incomingOrder->get_quantity()) + ", Available: " + std::to_string(availableQuantityAtBestPrice));
      return; // FOK order cannot be fully filled at best price, so it's rejected
    }
  }

  while (incomingOrder->get_quantity() > 0) {
    std::optional<MarketData> bestOpposingLevel = (incomingOrder->get_side() == Side::BUY) ? book.get_best_ask() : book.get_best_bid();

    if (!bestOpposingLevel) {
        LOG_DEBUG("match_order: No opposing side for order " + std::to_string(incomingOrder->get_order_id()));
        break;
    }

    if (incomingOrder->get_order_type() == OrderType::LIMIT) {
      auto limitOrder = static_cast<LimitOrder*>(incomingOrder);
      Price limitPrice = limitOrder->get_price();
      Price bestOpposingPrice = bestOpposingLevel->price;

      if ((incomingOrder->get_side() == Side::BUY && limitPrice < bestOpposingPrice) ||
          (incomingOrder->get_side() == Side::SELL && limitPrice > bestOpposingPrice)) {
        LOG_DEBUG("match_order: LIMIT order " + std::to_string(incomingOrder->get_order_id()) + " price not aggressive enough to match.");
        break;
      }
    }
        
    LOG_DEBUG("match_order: Iterating orders at price level " + format_price(bestOpposingLevel->price));
    // Use the new callback-based forEachOrderAtPrice
    bool continueMatchingAtPriceLevel = book.for_each_order_at_price(bestOpposingLevel->price,
      incomingOrder->get_side() == Side::BUY ? Side::SELL : Side::BUY,
      [&](OrderID restingOrderID) {
        Order* restingOrder = book.get_order(restingOrderID); 
        if (!restingOrder) {
            LOG_DEBUG("match_order: Resting order " + std::to_string(restingOrderID) + " not found in book, skipping.");
            return true; // Continue iteration if order not found
        }

        LOG_DEBUG("match_order: Considering match between incoming " + std::to_string(incomingOrder->get_order_id()) + " and resting " + std::to_string(restingOrderID));

        // Self Match Prevention
        if (restingOrder->get_trader_id() == incomingOrder->get_trader_id()) {
          LOG_DEBUG("match_order: SELF-MATCH DETECTED. Cancelling resting order " + std::to_string(restingOrderID));
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

        LOG_DEBUG("match_order: Creating trade between " + std::to_string(incomingOrder->get_order_id()) + " and " + std::to_string(restingOrderID) + " for qty " + std::to_string(tradeQuantity));
        create_trade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

        incomingOrder->set_quantity(incomingOrder->get_quantity() - tradeQuantity);
        book.reduce_order_quantity(restingOrderID, tradeQuantity); 

        if (incomingOrder->get_quantity() == 0) {
          LOG_DEBUG("match_order: Incoming order " + std::to_string(incomingOrder->get_order_id()) + " is now fully filled.");
          return false; // Aggressor fully filled, stop iterating orders at this price level
        }
        return true; // Continue iteration
      });

    // If forEachOrderAtPrice returned false because the incoming order was fully filled,
    // the while loop condition will handle breaking out. No explicit 'break' needed here.
  }
}

void MatchingEngine::place_resting_limit_order(std::shared_ptr<LimitOrder> order, OrderBook& book) {
  order->set_order_status(OrderStatus::ACCEPTED);
  event_dispatcher_.publish(OrderAcceptedEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_side(), order->get_price(), order->get_quantity()));
  book.add_order(std::move(order));
}

void MatchingEngine::create_trade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {
  LOG_DEBUG("create_trade: aggressor=" + std::to_string(aggressor->get_order_id()) + ", resting=" + std::to_string(resting->get_order_id()) + ", price=" + format_price(tradePrice) + ", qty=" + std::to_string(tradeQuantity));
  Quantity aggressorRemaining = aggressor->get_quantity() - tradeQuantity;
  Quantity restingRemaining = resting->get_quantity() - tradeQuantity;
  aggressor->set_order_status(aggressorRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
  resting->set_order_status(restingRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);

  event_dispatcher_.publish(TradeExecutedEvent(aggressor->get_symbol_id(), tradePrice, tradeQuantity, 
                            aggressor->get_order_id(), aggressor->get_trader_id(), aggressor->get_side(), aggressor->get_quantity(), 
                            resting->get_order_id(), resting->get_trader_id(), resting->get_quantity()));

  if (aggressorRemaining == 0) {
    event_dispatcher_.publish(OrderFilledEvent(aggressor->get_order_id(), aggressor->get_trader_id(), tradePrice));
    LOG_DEBUG("create_trade: aggressor " + std::to_string(aggressor->get_order_id()) + " filled, removing from order_id_to_symbol map");
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    order_id_to_symbol_.erase(aggressor->get_order_id());
  }
  if (restingRemaining == 0) {
    event_dispatcher_.publish(OrderFilledEvent(resting->get_order_id(), resting->get_trader_id(), tradePrice));
    LOG_DEBUG("create_trade: resting " + std::to_string(resting->get_order_id()) + " filled, removing from order_id_to_symbol map");
    std::lock_guard<std::mutex> lock(order_map_mutex_);
    order_id_to_symbol_.erase(resting->get_order_id());
  }

  trigger_stop_orders(aggressor->get_symbol_id(), tradePrice);
}

void MatchingEngine::trigger_stop_orders(SymbolID symbol_id, Price last_trade_price) {
  LOG_DEBUG("trigger_stop_orders: Checking for symbol " + std::to_string(symbol_id) + " at price " + format_price(last_trade_price));
  std::vector<OrderID> triggered_ids;
    
  {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    auto symbol_stops_it = stop_orders_by_symbol_.find(symbol_id);
    if (symbol_stops_it == stop_orders_by_symbol_.end()) {
      return; // No stop orders for this symbol
    }
    auto& stop_orders = symbol_stops_it->second;

    // Trigger buy stops (stop price <= last trade price)
    for (auto it = stop_orders.buy_stops.begin(); it != stop_orders.buy_stops.end() && it->first <= last_trade_price; ) {
      LOG_DEBUG("trigger_stop_orders: Triggering buy stops at price " + format_price(it->first));
      triggered_ids.insert(triggered_ids.end(), it->second.begin(), it->second.end());
      it = stop_orders.buy_stops.erase(it);
    }

    // Trigger sell stops (stop price >= last trade price)
    for (auto it = stop_orders.sell_stops.begin(); it != stop_orders.sell_stops.end() && it->first >= last_trade_price; ) {
      LOG_DEBUG("trigger_stop_orders: Triggering sell stops at price " + format_price(it->first));
      triggered_ids.insert(triggered_ids.end(), it->second.begin(), it->second.end());
      it = stop_orders.sell_stops.erase(it);
    }
  }

  // Activate the triggered orders 
  if (!triggered_ids.empty()) {
    LOG_DEBUG("trigger_stop_orders: Activating " + std::to_string(triggered_ids.size()) + " triggered orders.");
    std::lock_guard<std::mutex> untriggered_lock(untriggered_orders_mutex_);
    for (OrderID id : triggered_ids) {
      auto order_it = untriggered_orders_.find(id);
      if (order_it != untriggered_orders_.end()) {
        std::shared_ptr<Order> triggered_order = std::move(order_it->second);
        untriggered_orders_.erase(order_it);
              
        if (triggered_order->get_order_type() == OrderType::STOP_MARKET) {
          auto* stop_order = static_cast<StopMarketOrder*>(triggered_order.get());
          LOG_DEBUG("trigger_stop_orders: Activating STOP_MARKET " + std::to_string(id) + " as new MARKET order.");
          event_queue_.push(std::make_shared<MarketOrder>(
                      stop_order->get_symbol_id(), stop_order->get_order_id(), 
                      stop_order->get_side(), stop_order->get_quantity(), stop_order->get_trader_id()));
        } 
        else if (triggered_order->get_order_type() == OrderType::STOP_LIMIT) {
          auto* stop_order = static_cast<StopLimitOrder*>(triggered_order.get());
          LOG_DEBUG("trigger_stop_orders: Activating STOP_LIMIT " + std::to_string(id) + " as new LIMIT order.");
          event_queue_.push(std::make_shared<LimitOrder>(
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

bool MatchingEngine::is_running() const {
  return running_;
}

size_t MatchingEngine::get_event_queue_depth() const {
    return event_queue_.size();
}