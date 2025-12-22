/*
 * @file orderBook.h
 * @brief Defines the OrderBook class for managing orders for a single instrument.
 */
#ifndef TRADINGENGINE_INCLUDE_ORDERBOOK_H_
#define TRADINGENGINE_INCLUDE_ORDERBOOK_H_

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "order.h"
#include "utils.h"

/*
 * TODO: Utilize lock-free data structures and resource pooling.
 * - For Event Queues: Consider replacing ThreadSafeQueue with `moodycamel::ConcurrentQueue`.
 * - For Hash Maps (`all_orders_`, `order_iterators_`): Consider high-performance
 *   concurrent hash maps like `folly::ConcurrentHashMap` or `tbb::concurrent_hash_map`.
 * - For Sorted Price Maps (`bids_`, `asks_`): Lock-free sorted maps are complex.
 *   This might involve using a concurrent skip list (`folly::ConcurrentSkipList`)
 *   or a custom data structure designed for order book mechanics.
 * - For Order Objects: Resource pooling should be implemented in the order factory
 *   to avoid heap allocation overhead on order creation.
 */

struct MarketData {
  Price price;
  Quantity quantity;
};

class OrderBook {

private:

  struct PriceLevel {
    Quantity total_quantity;
    std::list<OrderID> orders;
  };

  mutable std::shared_mutex bids_mtx_;
  mutable std::shared_mutex asks_mtx_;
  mutable std::shared_mutex orders_mtx_;

  std::map<Price, PriceLevel, std::greater<Price>> bids_; // Sorted high to low
  std::map<Price, PriceLevel> asks_;                      // Sorted low to high
  std::unordered_map<OrderID, std::unique_ptr<Order>> all_orders_; // O(1) lookup for Order data
  std::unordered_map<OrderID, std::list<OrderID>::iterator> order_iterators_; // O(1) removal from PriceLevel list

  void remove_order(std::unordered_map<OrderID, std::unique_ptr<Order>>::iterator all_orders_it);

public:

  void add_order(std::unique_ptr<LimitOrder> order);
  void cancel_order(OrderID order_id);
  Order* get_order(OrderID order_id);
  void reduce_order_quantity(OrderID order_id, Quantity quantity_to_reduce);
  std::optional<MarketData> get_best_bid();
  std::optional<MarketData> get_best_ask();
  bool for_each_order_at_price(Price price, Side side, const std::function<bool(OrderID)>& callback);
  bool is_empty();
  bool is_side_empty(Side side);
  void print(int num_price_levels) const;

};

#endif // TRADINGENGINE_INCLUDE_ORDERBOOK_H_
