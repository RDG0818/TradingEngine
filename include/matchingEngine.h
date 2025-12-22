// include/matchingEngine.h

#ifndef TRADINGENGINE_INCLUDE_MATCHINGENGINE_H_
#define TRADINGENGINE_INCLUDE_MATCHINGENGINE_H_

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "eventDispatcher.h"
#include "eventQueue.h"
#include "events.h"
#include "order.h"
#include "orderBook.h"
#include "orderFactory.h"
#include "utils.h"

class MatchingEngine {

public:

  explicit MatchingEngine(EventDispatcher& event_dispatcher);
  ~MatchingEngine();

  virtual OrderID submit_order(const RawOrderParams& params);  
  void cancel_order(OrderID order_id);
  virtual std::optional<MarketData> get_best_ask(SymbolID symbol_id) const;
  virtual std::optional<MarketData> get_best_bid(SymbolID symbol_id) const;
  void print_top_of_book(std::string symbol, int num_price_levels);

  void start();
  void stop();

  // --- Test-only Methods ---
  // TODO(test): Revisit if this method should return OrderBook* or a const OrderBook*
  OrderBook* get_book(SymbolID symbol_id);

private:

  EventDispatcher& event_dispatcher_;
  std::atomic<OrderID> next_order_id_;

  std::unordered_map<SymbolID, std::unique_ptr<OrderBook>> books_;
  mutable std::shared_mutex books_mutex_;

  std::unordered_map<OrderID, SymbolID> order_id_to_symbol_;
  mutable std::mutex order_map_mutex_; 

  std::thread worker_thread_;
  std::atomic<bool> running_{false};
  ThreadSafeQueue<EngineEvent> event_queue_;

  // A map to own the memory of stop orders that have not yet been triggered
  std::unordered_map<OrderID, std::unique_ptr<Order>> untriggered_orders_;
  mutable std::mutex untriggered_orders_mutex_;

  // Stop orders are segregated by symbol for correctness and performance
  struct StopOrders {
      std::map<Price, std::list<OrderID>> buy_stops;
      std::map<Price, std::list<OrderID>> sell_stops;
  };
  std::unordered_map<SymbolID, StopOrders> stop_orders_by_symbol_;
  mutable std::mutex stop_orders_mutex_;

  // Private helper methods
  void run_loop();
  void process_order_submission(std::unique_ptr<Order> order);
  void process_order_cancellation(OrderID order_id);
  void process_stop_market_order(std::unique_ptr<StopMarketOrder> order);
  void process_stop_limit_order(std::unique_ptr<StopLimitOrder> order);
  void match_order(Order* incoming_order, OrderBook& book);
  void place_resting_limit_order(std::unique_ptr<LimitOrder> order, OrderBook& book);
  void create_trade(Order* aggressor, Order* resting, Price trade_price, Quantity trade_quantity);
  void trigger_stop_orders(SymbolID symbol_id, Price last_trade_price);
  OrderBook* get_or_create_order_book(SymbolID symbol_id);
};

#endif // TRADINGENGINE_INCLUDE_MATCHINGENGINE_H_


